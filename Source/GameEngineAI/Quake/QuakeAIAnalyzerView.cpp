//========================================================================
// QuakeAIAnalyzerView.cpp : Game View Class
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

#include "Quake.h"
#include "QuakeApp.h"
#include "QuakeAIView.h"
#include "QuakeAIManager.h"
#include "QuakeAIAnalyzerView.h"

#include "QuakeEvents.h"
#include "QuakeNetwork.h"
#include "QuakeLevelManager.h"
#include "QuakePlayerController.h"
#include "QuakeCameraController.h"

#include "Application/Settings.h"

#include "Audio/SoundOpenal.h"
#include "Audio/SoundProcess.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"
#include "Graphic/UI/UIEngine.h"

#include "Graphic/Scene/Scene.h"

#include "Core/OS/OS.h"
#include "Core/Utility/SHA1.h"
#include "Core/Utility/Profiler.h"
#include "Core/Utility/Serialize.h"

#include "Core/Event/Event.h"
#include "Core/Event/EventManager.h"

#include "Physic/PhysicEventListener.h"

#include "Game/View/HumanView.h"

#include "Graphics/GraphNode.h"
#include "Graphics/PathNode.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/RenderComponent.h"

namespace AIAnalyzer
{
	/*
		Draws a screen with a single text on it.
		Text will be removed when the screen is drawn the next time.
		Additionally, a progressbar can be drawn when percent is set between 0 and 100.
	*/
	void DrawLoadScreen(const std::wstring& text, std::shared_ptr<BaseUI> ui,
		std::shared_ptr<Visual> visual, std::shared_ptr<BlendState> blendState, int percent)
	{
		Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

		Vector2<int> texSize = ui->GetSkin()->GetFont()->GetDimension(text);
		Vector2<int> center{ (int)screenSize[0] / 2, (int)screenSize[1] / 2 };
		RectangleShape<2, int> textRect;
		textRect.mExtent = texSize;
		textRect.mCenter = center;

		std::shared_ptr<BaseUIStaticText> uiText = ui->AddStaticText(text.c_str(), textRect, false, false);
		uiText->SetTextAlignment(UIA_CENTER, UIA_UPPERLEFT);

		Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
		Renderer::Get()->ClearBuffers();

		// draw progress bar
		if ((percent >= 0) && (percent <= 100))
		{
			std::string texturePath = "art/quake/textures/";

			std::shared_ptr<Texture2> progressImg;
			if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar.png")))
			{
				std::shared_ptr<ResHandle> resHandle =
					ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar.png")));
				if (resHandle)
				{
					std::shared_ptr<ImageResourceExtraData> resData =
						std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
					if (resData)
					{
						progressImg = resData->GetImage();
						progressImg->AutogenerateMipmaps();
					}
				}
			}

			std::shared_ptr<Texture2> progressImgBG;
			if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar_bg.png")))
			{
				std::shared_ptr<ResHandle> resHandle =
					ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar_bg.png")));
				if (resHandle)
				{
					std::shared_ptr<ImageResourceExtraData> resData =
						std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
					if (resData)
					{
						progressImgBG = resData->GetImage();
						progressImgBG->AutogenerateMipmaps();
					}
				}
			}

			if (progressImg && progressImgBG)
			{
				Renderer::Get()->SetBlendState(blendState);

				int imgW = std::clamp((int)progressImgBG->GetDimension(0), 200, 600);
				int imgH = std::clamp((int)progressImgBG->GetDimension(1), 24, 72);

				Vector2<int> imgPos{ ((int)screenSize[0] - imgW) / 2, ((int)screenSize[1] - imgH) / 2 };

				auto effect = std::dynamic_pointer_cast<Texture2Effect>(visual->GetEffect());
				effect->SetTexture(progressImgBG);

				RectangleShape<2, int> rect;
				rect.mExtent = Vector2<int>{ imgW, imgH };
				rect.mCenter = rect.mExtent / 2 + imgPos;

				RectangleShape<2, int> tcoordRect;
				tcoordRect.mExtent = Vector2<int>{
					(int)effect->GetTexture()->GetDimension(0) ,
					(int)effect->GetTexture()->GetDimension(1) };
				tcoordRect.mCenter = tcoordRect.mExtent / 2;

				ui->GetSkin()->Draw2DTextureFilterScaled(visual, rect, tcoordRect);

				effect->SetTexture(progressImg);

				Vector2<int> offset = Vector2<int>{ 2, 0 };
				rect.mExtent = Vector2<int>{ (percent * imgW) / 100, imgH } - offset;
				rect.mCenter = rect.mExtent / 2 + imgPos + offset;

				tcoordRect.mExtent = Vector2<int>{
					(percent * (int)effect->GetTexture()->GetDimension(0)) / 100 ,
					(int)effect->GetTexture()->GetDimension(1) };
				tcoordRect.mCenter = tcoordRect.mExtent / 2;

				ui->GetSkin()->Draw2DTextureFilterScaled(visual, rect, tcoordRect);

				Renderer::Get()->SetDefaultBlendState();
			}
		}

		ui->BaseUI::OnRender(0, 0);
		uiText->Remove();

		Renderer::Get()->DisplayColorBuffer(0);
	}

	/*
	On some computers framerate doesn't seem to be automatically limited
	 */
	void LimitFps(FpsControl* fpsTimings, float* dTime)
	{
		// not using getRealTime is necessary for wine
		Timer::Tick(); // Maker sure device time is up-to-date
		unsigned int time = Timer::GetTime();
		unsigned int lastTime = fpsTimings->lastTime;

		if (time > lastTime)  // Make sure time hasn't overflowed
			fpsTimings->busyTime = time - lastTime;
		else
			fpsTimings->busyTime = 0;

		unsigned int frameTimeMin = (unsigned int)(1000 / (System::Get()->IsWindowFocused() ?
			Settings::Get()->GetFloat("fps_max") : Settings::Get()->GetFloat("fps_max_unfocused")));

		if (fpsTimings->busyTime < frameTimeMin)
		{
			fpsTimings->sleepTime = frameTimeMin - fpsTimings->busyTime;
			Sleep(fpsTimings->sleepTime);
		}
		else fpsTimings->sleepTime = 0;

		/* Get the new value of the device timer. Note that device->sleep() may
		 * not sleep for the entire requested time as sleep may be interrupted and
		 * therefore it is arguably more accurate to get the new time from the
		 * device rather than calculating it by adding sleep_time to time.
		 */

		Timer::Tick(); // Update device timer
		time = Timer::GetTime();

		if (time > lastTime)  // Make sure last_time hasn't overflowed
			*dTime = (time - lastTime) / 1000.f;
		else
			*dTime = 0;

		fpsTimings->lastTime = time;
	}
}

void AIAnalyzerSoundFetcher::PathsInsert(std::set<std::string>& dstPaths,
	const std::string& base, const std::string& name)
{
	std::wstring soundPath =
		FileSystem::Get()->GetWorkingDirectory() + L"/../../Assets/Art/Quake/audio";

	std::vector<std::wstring> paths = 
		FileSystem::Get()->GetRecursiveDirectories(soundPath);
	for (const auto& path : paths)
	{
		if (FileSystem::Get()->ExistFile(path + L"/" + ToWideString(name.c_str()) + L".ogg"))
		{
			std::string filePath = ToString(path.substr(soundPath.size()).c_str());
			dstPaths.insert(base + filePath + "/" + name + ".ogg");
			break;
		}
		if (FileSystem::Get()->ExistFile(path + L"/" + ToWideString(name.c_str()) + L".wav"))
		{
			std::string filePath = ToString(path.substr(soundPath.size()).c_str());
			dstPaths.insert(base + filePath + "/" + name + ".wav");
			break;
		}
	}
}

void  AIAnalyzerSoundFetcher::FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
{
	if (mFetched.count(name))
		return;

	mFetched.insert(name);

	std::string soundBase = "art/quake/audio";
	PathsInsert(dstPaths, soundBase, name);
}

//========================================================================
//
// QuakeAIAnalyzerUI implementation
//
//========================================================================

QuakeAIAnalyzerUI::QuakeAIAnalyzerUI(const QuakeAIAnalyzerView* view) : mAIAnalyzerView(view)
{
	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

	if (GetSkin())
		mStatusTextInitialColor = GetSkin()->GetColor(DC_BUTTON_TEXT);
	else
		mStatusTextInitialColor = SColor(255, 0, 0, 0);
}

QuakeAIAnalyzerUI::~QuakeAIAnalyzerUI()
{ 

}

bool QuakeAIAnalyzerUI::OnInit()
{
	BaseUI::OnInit();

	// set a nicer font
	const std::shared_ptr<BaseUIFont>& font = GetFont(L"DefaultFont");
	if (font) GetSkin()->SetFont(font);

	GetSkin()->SetColor(DC_BUTTON_TEXT, SColor(255, 255, 255, 255));
	GetSkin()->SetColor(DC_3D_LIGHT, SColor(0, 0, 0, 0));
	GetSkin()->SetColor(DC_3D_HIGH_LIGHT, SColor(255, 30, 30, 30));
	GetSkin()->SetColor(DC_3D_SHADOW, SColor(255, 0, 0, 0));
	GetSkin()->SetColor(DC_HIGH_LIGHT, SColor(255, 70, 120, 50));
	GetSkin()->SetColor(DC_HIGH_LIGHT_TEXT, SColor(255, 255, 255, 255));
	GetSkin()->SetColor(DC_EDITABLE, SColor(255, 128, 128, 128));
	GetSkin()->SetColor(DC_FOCUSED_EDITABLE, SColor(255, 96, 134, 49));

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		// Create a vertex buffer for a two-triangles square. The PNG is stored
		// in left-handed coordinates. The texture coordinates are chosen to
		// reflect the texture in the y-direction.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
		vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		// Create an effect for the vertex and pixel shaders. The texture is
		// bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extraRes =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extraRes->GetProgram())
			extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		auto effect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
	}

	// First line of debug text
	mText = AddStaticText(L"Quake", RectangleShape<2, int>(), false, false);

	// Second line of debug text
	mText2 = AddStaticText(L"", RectangleShape<2, int>(), false, false);

	// Chat text
	mTextChat = AddStaticText(L"", RectangleShape<2, int>(), false, true);

	uint16_t chatFontSize = Settings::Get()->GetUInt16("chat_font_size");
	if (chatFontSize != 0)
		mTextChat->SetOverrideFont(GetBuiltInFont());
	//g_fontengine->getFont(chatFontSize, FM_UNSPECIFIED));

	// At the middle of the screen Object infos are shown in this
	int chatFontHeight = mTextChat->GetActiveFont()->GetDimension(L"Ay")[1];
	uint16_t recentChatMessages = Settings::Get()->GetUInt16("recent_chat_messages");

	RectangleShape<2, int> rect;
	rect.mExtent[0] = 400;
	rect.mExtent[1] = chatFontHeight * 5 + 5;
	rect.mCenter = rect.mExtent / 2;
	rect.mCenter += Vector2<int>{100, chatFontHeight* (recentChatMessages + 3)};
	mTextInfo = AddStaticText(L"", rect, false, true);

	// Status text (displays info when showing and hiding GUI stuff, etc.)
	mTextStatus = AddStaticText(
		L"<Status>", RectangleShape<2, int>(), false, false);
	mTextStatus->SetVisible(false);

	// Profiler text (size is updated when text is updated)
	mTextProfiler = AddStaticText(
		L"<Profiler>", RectangleShape<2, int>(), false, true);
	mTextProfiler->SetOverrideFont(GetBuiltInFont());
	/*
	mTextProfiler->SetOverrideFont(g_fontengine->getFont(
		g_fontengine->getDefaultFontSize() * 0.9f, FM_MONO));
	*/
	mTextProfiler->SetVisible(false);

	// Chat backend and console
	rect.mExtent = Vector2<int>{ 100, 100 };
	rect.mCenter = rect.mExtent / 2;
	mChatConsole = std::make_shared<UIChatConsole>(this, -1, rect);
	mChatConsole->SetParent(GetRootUIElement());
	mChatConsole->SetVisible(false);

	mFlags = QuakeAIAnalyzerUI::Flags();
	mFlags.showDebug = Settings::Get()->GetBool("show_debug");

	return true;
}

void QuakeAIAnalyzerUI::Update(
	const AIAnalyzer::RunStats& stats, const std::shared_ptr<CameraNode> camera,
	const std::shared_ptr<UIChatConsole> chatConsole, float dTime)
{
	Vector2<unsigned int> screensize = Renderer::Get()->GetScreenSize();

	if (mFlags.showDebug)
	{
		unsigned short fps = (unsigned short)(1.f / stats.dTimeJitter.avg);

		if (mFlags.showAnalysis)
		{
			std::ostringstream os(std::ios_base::binary);
			os << std::fixed
				<< "Quake " << " | FPS: " << fps
				<< std::setprecision(0)
				<< " | time: " + stats.gameTime
				<< " | dtime jitter: "
				<< (stats.dTimeJitter.maxFraction * 100.0) << "%"
				<< std::setprecision(1);

			//<< std::setprecision(2)
			//<< " | RTT: " << (client->getRTT() * 1000.0f) << "ms";
			mText->SetText(ToWideString(os.str()).c_str());
		}
		else
		{
			Timer::RealTimeDate realTime = Timer::GetRealTimeAndDate();

			std::ostringstream os(std::ios_base::binary);
			os << std::fixed
				<< "Quake " << " | FPS: " << fps
				<< std::setprecision(0)
				<< " | time: " +
				std::to_string(realTime.Hour) + ":" +
				std::to_string(realTime.Minute) + ":" +
				std::to_string(realTime.Second)
				<< " | dtime jitter: "
				<< (stats.dTimeJitter.maxFraction * 100.0) << "%"
				<< std::setprecision(1);

			//<< std::setprecision(2)
			//<< " | RTT: " << (client->getRTT() * 1000.0f) << "ms";
			mText->SetText(ToWideString(os.str()).c_str());
		}

		int chatFontHeight = mTextChat->GetActiveFont()->GetDimension(L"Ay")[1];

		RectangleShape<2, int> rect;
		rect.mExtent[0] = screensize[0] - 5;
		rect.mExtent[1] = chatFontHeight;
		rect.mCenter = rect.mExtent / 2 + Vector2<int>{5, 5};
		mText->SetRelativePosition(rect);
	}

	// Finally set the guitext visible depending on the flag
	mText->SetVisible(mFlags.showDebug);

	if (mFlags.showDebug)
	{		
		EulerAngles<float> rotation;
		rotation.mAxis[1] = 1;
		rotation.mAxis[2] = 2;
		camera->GetAbsoluteTransform().GetRotation(rotation);
		Vector3<float> position = camera->GetAbsoluteTransform().GetTranslation();

		std::ostringstream os(std::ios_base::binary);
		os << std::setprecision(1) << std::fixed
			<< "pos: (" << position[0] << ", " << position[1] << ", " << position[2]
			<< ") | yaw: " << rotation.mAngle[2] << " "
			<< " | pitch: " << rotation.mAngle[1];
		//<< " | seed: " << ((uint64_t)client->GetMapSeed());

		mText2->SetText(ToWideString(os.str()).c_str());
		int chatFontHeight = mTextChat->GetActiveFont()->GetDimension(L"Ay")[1];

		RectangleShape<2, int> rect;
		rect.mExtent[0] = screensize[0] - 5;
		rect.mExtent[1] = chatFontHeight;
		rect.mCenter = rect.mExtent / 2 + Vector2<int>{5, 5 + chatFontHeight};
		mText2->SetRelativePosition(rect);
	}

	mText2->SetVisible(mFlags.showDebug);

	mTextInfo->SetText(mInfoText.c_str());
	mTextInfo->SetVisible(mFlags.showHud);

	static const float statusTextTimeMax = 1.5f;
	if (!mStatusText.empty())
	{
		mStatusTextTime += dTime;

		if (mStatusTextTime >= statusTextTimeMax)
		{
			ClearStatusText();
			mStatusTextTime = 0.0f;
		}
	}

	mTextStatus->SetText(mStatusText.c_str());
	mTextStatus->SetVisible(!mStatusText.empty());

	if (!mStatusText.empty())
	{
		int statusWidth = mTextStatus->GetTextWidth();
		int statusHeight = mTextStatus->GetTextHeight();
		int statusY = screensize[1] - 150;
		int statusX = (screensize[0] - statusWidth) / 2;

		RectangleShape<2, int> rect;
		rect.mExtent[0] = statusWidth;
		rect.mExtent[1] = statusHeight;
		rect.mCenter[0] = rect.mExtent[0] / 2 + statusX;
		rect.mCenter[1] = -rect.mExtent[1] / 2 + statusY;
		mTextStatus->SetRelativePosition(rect);

		// Fade out
		SColor finalColor = mStatusTextInitialColor;
		finalColor.SetAlpha(0);
		SColor fadeColor = mStatusTextInitialColor.GetInterpolatedQuadratic(
			mStatusTextInitialColor, finalColor, mStatusTextTime / statusTextTimeMax);
		mTextStatus->SetOverrideColor(fadeColor);
		mTextStatus->EnableOverrideColor(true);
	}

	// Hide chat when console is visible
	mTextChat->SetVisible(IsChatVisible() && !chatConsole->IsVisible());
}

void QuakeAIAnalyzerUI::ShowTranslatedStatusText(const char* str)
{
	ShowStatusText(ToWideString(str));
}

void QuakeAIAnalyzerUI::SetChatText(const EnrichedString& chatText, unsigned int recentChatCount)
{

	// Update gui element size and position
	int chatY = 5;
	int chatFontHeight = mTextChat->GetActiveFont()->GetDimension(L"Ay")[1];
	if (mFlags.showDebug)
		chatY += 2 * chatFontHeight;

	const Vector2<unsigned int>& windowSize = Renderer::Get()->GetScreenSize();

	RectangleShape<2, int> chatSize;
	chatSize.mExtent[0] = windowSize[0] - 30;
	chatSize.mExtent[1] = std::min((int)windowSize[1], mTextChat->GetTextHeight() + chatY);
	chatSize.mCenter[0] = chatSize.mExtent[0] / 2 + 10;
	chatSize.mCenter[1] = chatSize.mExtent[1] / 2;

	mTextChat->SetRelativePosition(chatSize);
	mTextChat->SetText(chatText.C_Str());

	mRecentChatCount = recentChatCount;
}

void QuakeAIAnalyzerUI::UpdateProfiler()
{
	if (mProfilerCurrentPage != 0)
	{
		std::ostringstream os(std::ios_base::binary);
		os << "   Profiler page " << (int)mProfilerCurrentPage <<
			", elapsed: " << Profiling->GetElapsedTime() << " ms)" << std::endl;

		int lines = Profiling->Print(os, mProfilerCurrentPage, mProfilerMaxPage);
		++lines;

		EnrichedString str(ToWideString(os.str()));
		str.SetBackground(SColor(120, 0, 0, 0));

		Vector2<int> size = mTextProfiler->GetOverrideFont()->GetDimension(str.C_Str());
		Vector2<int> upperLeft{ 6, 50 };
		Vector2<int> lowerRight = upperLeft;
		lowerRight[0] += size[0] + 10;
		lowerRight[1] += size[1];

		RectangleShape<2, int> rect;
		rect.mExtent = lowerRight - upperLeft;
		rect.mCenter = upperLeft + rect.mExtent / 2;
		mTextProfiler->SetRelativePosition(rect);

		mTextProfiler->SetDrawBackground(true);
		mTextProfiler->SetBackgroundColor(str.GetBackground());
		mTextProfiler->SetText(str.C_Str());
	}

	mTextProfiler->SetVisible(mProfilerCurrentPage != 0);
}

void QuakeAIAnalyzerUI::ToggleChat()
{
	mFlags.showChat = !mFlags.showChat;
	if (mFlags.showChat)
		ShowTranslatedStatusText("Chat shown");
	else
		ShowTranslatedStatusText("Chat hidden");
}

void QuakeAIAnalyzerUI::ToggleHud()
{
	mFlags.showHud = !mFlags.showHud;
	if (mFlags.showHud)
		ShowTranslatedStatusText("HUD shown");
	else
		ShowTranslatedStatusText("HUD hidden");
}

void QuakeAIAnalyzerUI::ToggleProfiler()
{
	mProfilerCurrentPage = (mProfilerCurrentPage + 1) % (mProfilerMaxPage + 1);

	// FIXME: This updates the profiler with incomplete values
	UpdateProfiler();

	if (mProfilerCurrentPage != 0)
	{
		wchar_t buf[255];
		wsprintf(buf, L"Profiler shown (page %d of %d)", mProfilerCurrentPage, mProfilerMaxPage);
		ShowStatusText(buf);
	}
	else ShowTranslatedStatusText("Profiler hidden");
}

/*
	Draws a screen with a single text on it.
	Text will be removed when the screen is drawn the next time.
	Additionally, a progressbar can be drawn when percent is set between 0 and 100.
*/
void QuakeAIAnalyzerUI::ShowOverlayMessage(const std::wstring& text,
	float dTime, int percent, bool drawClouds)
{
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();

	Vector2<int> texSize = mTextChat->GetActiveFont()->GetDimension(text);
	Vector2<int> center{ (int)screenSize[0] / 2, (int)screenSize[1] / 2 };
	RectangleShape<2, int> textRect;
	textRect.mExtent = texSize;
	textRect.mCenter = center;

	std::shared_ptr<BaseUIStaticText> uiText = AddStaticText(text.c_str(), textRect, false, false);
	uiText->SetTextAlignment(UIA_CENTER, UIA_UPPERLEFT);

	// draw progress bar
	if ((percent >= 0) && (percent <= 100))
	{
		std::string texturePath = "art/quake/textures/";

		std::shared_ptr<Texture2> progressImg;
		if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar.png")))
		{
			std::shared_ptr<ResHandle> resHandle =
				ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar.png")));
			if (resHandle)
			{
				std::shared_ptr<ImageResourceExtraData> resData =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				if (resData)
				{
					progressImg = resData->GetImage();
					progressImg->AutogenerateMipmaps();
				}
			}
		}

		std::shared_ptr<Texture2> progressImgBG;
		if (FileSystem::Get()->ExistFile(ToWideString(texturePath + "progress_bar_bg.png")))
		{
			std::shared_ptr<ResHandle> resHandle =
				ResCache::Get()->GetHandle(&BaseResource(ToWideString(texturePath + "progress_bar_bg.png")));
			if (resHandle)
			{
				std::shared_ptr<ImageResourceExtraData> resData =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				if (resData)
				{
					progressImgBG = resData->GetImage();
					progressImgBG->AutogenerateMipmaps();
				}
			}
		}

		if (progressImg && progressImgBG)
		{
			Renderer::Get()->SetBlendState(mBlendState);

			int imgW = std::clamp((int)progressImgBG->GetDimension(0), 200, 600);
			int imgH = std::clamp((int)progressImgBG->GetDimension(1), 24, 72);

			Vector2<int> imgPos{ ((int)screenSize[0] - imgW) / 2, ((int)screenSize[1] - imgH) / 2 };

			auto effect = std::dynamic_pointer_cast<Texture2Effect>(mVisual->GetEffect());
			effect->SetTexture(progressImgBG);

			RectangleShape<2, int> rect;
			rect.mExtent = Vector2<int>{ imgW, imgH };
			rect.mCenter = rect.mExtent / 2 + imgPos;

			RectangleShape<2, int> tcoordRect;
			tcoordRect.mExtent = Vector2<int>{
				(int)effect->GetTexture()->GetDimension(0) ,
				(int)effect->GetTexture()->GetDimension(1) };
			tcoordRect.mCenter = tcoordRect.mExtent / 2;

			GetSkin()->Draw2DTextureFilterScaled(mVisual, rect, tcoordRect);

			effect->SetTexture(progressImg);

			rect.mExtent = Vector2<int>{ (percent * imgW) / 100, imgH };
			rect.mCenter = rect.mExtent / 2 + imgPos;

			tcoordRect.mExtent = Vector2<int>{
				(percent * (int)effect->GetTexture()->GetDimension(0)) / 100 ,
				(int)effect->GetTexture()->GetDimension(1) };
			tcoordRect.mCenter = tcoordRect.mExtent / 2;

			GetSkin()->Draw2DTextureFilterScaled(mVisual, rect, tcoordRect);

			Renderer::Get()->SetDefaultBlendState();
		}
	}

	BaseUI::OnRender(0, 0);
	uiText->Remove();
}

bool QuakeAIAnalyzerUI::IsMenuActive()
{
	return mIsMenuActive;
}

void QuakeAIAnalyzerUI::SetMenuActive(bool active)
{
	mIsMenuActive = active;
}

bool QuakeAIAnalyzerUI::OnRestore()
{
	return BaseUI::OnRestore();
}

bool QuakeAIAnalyzerUI::OnRender(double time, float elapsedTime)
{
	TimeTaker ttDraw("Draw scene");

	ProfilerGraph graph = ProfilerGraph(this);

	Vector2<unsigned int> previousScreenSize{
		Settings::Get()->GetUInt16("screen_w"), Settings::Get()->GetUInt16("screen_h") };

	const Vector2<unsigned int>& currentScreenSize = Renderer::Get()->GetScreenSize();
	// Verify if window size has changed and save it if it's the case
	// Ensure evaluating settings->getBool after verifying screensize
	// First condition is cheaper
	if (previousScreenSize != currentScreenSize &&
		currentScreenSize != Vector2<unsigned int>::Zero() &&
		Settings::Get()->GetBool("autosave_screensize"))
	{
		Settings::Get()->SetUInt16("screen_w", currentScreenSize[0]);
		Settings::Get()->SetUInt16("screen_h", currentScreenSize[1]);
		previousScreenSize = currentScreenSize;
	}

	// Prepare render data for next iteration
	ClearInfoText();

	SColor skyColor = Renderer::Get()->GetClearColor().ToSColor();
	
	if (mAIAnalyzerView->mCamera->GetTarget())
	{
		std::shared_ptr<PlayerActor> player(std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(mAIAnalyzerView->mCamera->GetTarget()->GetId()).lock()));
		if (player->GetState().hudFlags & HUD_FLAG_CROSSHAIR_VISIBLE)
			mHud->DrawCrosshair(L"art/quake/gfx/2d/crosshair2.png");
		mHud->DrawElements(player);
	}

	/*
		Profiler graph
	*/
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
	if (mFlags.showProfilerGraph)
		graph.Draw(10, screenSize[1] - 10, GetBuiltInFont());

	/*
		Damage flash
	*/
	if (mDamageFlash > 0.0f)
	{
		RectangleShape<2, int> rect;
		rect.mExtent = Vector2<int>{ (int)screenSize[0], (int)screenSize[1] };
		rect.mCenter = rect.mExtent / 2;
		SColor color((uint32_t)mDamageFlash, 180, 0, 0);

		// Create a vertex buffer for a single triangle.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/ColorEffectVS.glsl");
		path.push_back("Effects/ColorEffectPS.glsl");
#else
		path.push_back("Effects/ColorEffectVS.hlsl");
		path.push_back("Effects/ColorEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

		// Create the geometric object for drawing.
		std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
		GetSkin()->Draw2DRectangle(color, visual, rect,
			&GetRootUIElement()->GetAbsoluteClippingRect());

		mDamageFlash -= 384.0f * elapsedTime / 1000.f;
	}

	if (!BaseUI::OnRender(time, elapsedTime))
		return false;

	Profiling->GraphAdd("Render frame [ms]", (float)ttDraw.Stop(true));

	/* Log times and stuff for visualization */
	Profiler::GraphValues values;
	Profiling->GraphGet(values);
	graph.Put(values);

	return true;
};

bool QuakeAIAnalyzerUI::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc( evt );
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool QuakeAIAnalyzerUI::OnEvent(const Event& evt)
{
	return false;
}


//========================================================================
//
// QuakeAIAnalyzerView Implementation
//
//========================================================================



void AIAnalyzerSettings::ReadGlobalSettings()
{
	doubletapJump = Settings::Get()->GetBool("doubletap_jump");
	enableClouds = Settings::Get()->GetBool("enable_clouds");
	enableParticles = Settings::Get()->GetBool("enable_particles");
	enableFog = Settings::Get()->GetBool("enable_fog");
	mouseSensitivity = Settings::Get()->GetFloat("mouse_sensitivity");
	repeatPlaceTime = Settings::Get()->GetFloat("repeat_place_time");

	enableNoclip = Settings::Get()->GetBool("noclip");
	enableFreeMove = Settings::Get()->GetBool("free_move");

	fogStart = Settings::Get()->GetFloat("fog_start");

	cameraSmoothing = 0;
	if (Settings::Get()->GetBool("cinematic"))
		cameraSmoothing = 1 - Settings::Get()->GetFloat("cinematic_camera_smoothing");
	else
		cameraSmoothing = 1 - Settings::Get()->GetFloat("camera_smoothing");

	fogStart = std::clamp(fogStart, 0.0f, 0.99f);
	cameraSmoothing = std::clamp(cameraSmoothing, 0.01f, 1.0f);
	mouseSensitivity = std::clamp(mouseSensitivity, 0.001f, 100.0f);
}

void QuakeAIAnalyzerView::SettingsChangedCallback(const std::string& name, void* data)
{
	((AIAnalyzerSettings*)data)->ReadGlobalSettings();
}


//
// QuakeAIAnalyzerView::QuakeAIAnalyzerView	- Chapter 19, page 724
//
QuakeAIAnalyzerView::QuakeAIAnalyzerView() : HumanView()
{ 
	mShowUI = true; 
	mDebugMode = DM_OFF;

	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

	std::shared_ptr<ResHandle>& resHandle = 
		ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		// Create a vertex buffer for a two-triangles square. The PNG is stored
		// in left-handed coordinates. The texture coordinates are chosen to
		// reflect the texture in the y-direction.
		VertexFormat vformat;
		vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
		vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
		std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
		vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

		// Create an effect for the vertex and pixel shaders. The texture is
		// bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extraRes =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extraRes->GetProgram())
			extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		auto effect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

		// Create the geometric object for drawing.
		mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
	}

    RegisterAllDelegates();

	mSettings.ReadGlobalSettings();
	// Register game setting callbacks
	for (const std::string& name : mSettings.settingNames)
	{
		Settings::Get()->RegisterChangedCallback(name,
			&QuakeAIAnalyzerView::SettingsChangedCallback, &mSettings);
	}
}


QuakeAIAnalyzerView::~QuakeAIAnalyzerView()
{
    RemoveAllDelegates();

	// mSettings becomes invalid, remove callbacks
	for (const std::string& name : mSettings.settingNames)
	{
		Settings::Get()->DeregisterChangedCallback(name,
			&QuakeAIAnalyzerView::SettingsChangedCallback, &mSettings);
	}

	Shutdown();
}

//
// QuakeAIAnalyzerView::OnMsgProc				- Chapter 19, page 727
//
bool QuakeAIAnalyzerView::OnMsgProc( const Event& evt )
{
    if (!mUI->mChatConsole->IsOpen())
    {
        switch (evt.mEventType)
        {
            case ET_UI_EVENT:
                // hey, why is the user sending gui events..?
                break;

            case ET_KEY_INPUT_EVENT:
            {
                if (evt.mKeyInput.mPressedDown)
                {
                    KeyAction key(evt.mKeyInput);
                    int keyType = mKeycache.Find(key);
                    if (keyType != -1)
                    {
                        if (!IsKeyDown((GameKeyType)keyType))
                            mKeyWasPressed.Set(mKeycache.keys[keyType]);

                        mKeyIsDown.Set(mKeycache.keys[keyType]);
                        mKeyWasDown.Set(mKeycache.keys[keyType]);
                    }
                }
                else
                {
                    KeyAction key(evt.mKeyInput);
                    int keyType = mKeycache.Find(key);
                    if (keyType != -1)
                    {
                        if (!IsKeyDown((GameKeyType)keyType))
                            mKeyWasReleased.Set(mKeycache.keys[keyType]);

                        mKeyIsDown.Unset(mKeycache.keys[keyType]);
                    }
                }
            }
            break;

            case ET_MOUSE_INPUT_EVENT:
            {
                KeyAction key;
                switch (evt.mMouseInput.mEvent) 
                {
                    case MIE_LMOUSE_PRESSED_DOWN:
                        key = "KEY_LBUTTON";
                        mKeyIsDown.Set(key);
                        mKeyWasDown.Set(key);
                        mKeyWasPressed.Set(key);
					    break;
                    case MIE_MMOUSE_PRESSED_DOWN:
                        key = "KEY_MBUTTON";
                        mKeyIsDown.Set(key);
                        mKeyWasDown.Set(key);
                        mKeyWasPressed.Set(key);
					    break;
                    case MIE_RMOUSE_PRESSED_DOWN:
                        key = "KEY_RBUTTON";
                        mKeyIsDown.Set(key);
                        mKeyWasDown.Set(key);
                        mKeyWasPressed.Set(key);
					    break;
                    case MIE_LMOUSE_LEFT_UP:
                        key = "KEY_LBUTTON";
                        mKeyIsDown.Unset(key);
                        mKeyWasReleased.Set(key);
                        break;
                    case MIE_MMOUSE_LEFT_UP:
                        key = "KEY_MBUTTON";
                        mKeyIsDown.Unset(key);
                        mKeyWasReleased.Set(key);
					    break;
                    case MIE_RMOUSE_LEFT_UP:
                        key = "KEY_RBUTTON";
                        mKeyIsDown.Unset(key);
                        mKeyWasReleased.Set(key);
                        break;
                    case MIE_MOUSE_WHEEL:
                        mMouseWheel = evt.mMouseInput.mWheel;
                        break;
                }
            }
            break;
        }

		if (mUI->mFormName != "PAUSE_MENU" && mUI->HasFocus(mUI->mForm, true))
		{
			if (IsKeyDown(KeyType::DIG))
			{
				Vector2<int> pos = Vector2<int>{ evt.mMouseInput.X, evt.mMouseInput.Y };
				if (!mUI->mForm->GetAbsolutePosition().IsPointInside(pos))
				{
					mUI->RemoveFocus(mUI->mForm);
					ClearInput();
				}
			}
		}
    }

    return HumanView::OnMsgProc(evt);
}

//
// QuakeAIAnalyzerView::RenderText				- Chapter 19, page 727
//
void QuakeAIAnalyzerView::RenderText()
{
	HumanView::RenderText();
}

//
// QuakeAIAnalyzerView::OnRender
//
void QuakeAIAnalyzerView::OnRender(double time, float elapsedTime)
{
	// Drawing begins
	Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));

	HumanView::OnRender(time, elapsedTime);
}

//
// QuakeAIAnalyzerView::OnUpdate				- Chapter 19, page 730
//
void QuakeAIAnalyzerView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate( timeMs, deltaMs );

	const Vector2<unsigned int>& currentScreenSize = Renderer::Get()->GetScreenSize();
	// Verify if window size has changed and save it if it's the case
	// Ensure evaluating settings->getBool after verifying screensize
	// First condition is cheaper
	if (mScreenSize != currentScreenSize &&
		currentScreenSize != Vector2<unsigned int>::Zero() &&
		Settings::Get()->GetBool("autosave_screensize"))
	{
		Settings::Get()->SetUInt16("screen_w", currentScreenSize[0]);
		Settings::Get()->SetUInt16("screen_h", currentScreenSize[1]);
		mScreenSize = currentScreenSize;
	}

	// Calculate dtime =
	//    RenderingEngine::run() from this iteration
	//  + Sleep time until the wanted FPS are reached
	LimitFps(&mUpdateTimes, &mDeltaTime);

	// Prepare render data for current iteration

	UpdateStats(&mStats, mUpdateTimes, mDeltaTime);

	UpdateProfilers(mStats, mUpdateTimes, mDeltaTime);
	ProcessUserInput(mDeltaTime);
	// Update camera before player movement to avoid camera lag of one frame
	UpdateControllers(timeMs, deltaMs);
	Step(mDeltaTime);
	UpdateSound(mDeltaTime);
	UpdateFrame(&mStats, mDeltaTime);

	if (Settings::Get()->GetBool("pause_on_lost_focus") &&
		!System::Get()->IsWindowFocused() &&
		!mUI->IsMenuActive())
	{
		ShowPauseMenu();
	}

	std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
	if (mUI->mFormName == "PAUSE_MENU" && formUI->IsActive())
		mUI->SetMenuActive(true);
	else
		mUI->SetMenuActive(false);
}

void QuakeAIAnalyzerView::Step(float dTime)
{
	mSoundMgr->Step(dTime);

	// Update positions of sounds attached to objects
	{
		for (auto const& soundsToObject : mSoundsToObjects)
		{
			int clientId = soundsToObject.first;
			uint16_t objectId = soundsToObject.second;
			std::shared_ptr<Actor> pActor = GameLogic::Get()->GetActor(objectId).lock();
			if (!pActor)
				continue;

			std::shared_ptr<TransformComponent> pTransformComponent(
				pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
				mSoundMgr->UpdateSoundPosition(clientId, pTransformComponent->GetPosition());
		}
	}

	// Handle removed remotely initiated sounds
	mRemoveSoundsCheckTimer += dTime;
	if (mRemoveSoundsCheckTimer >= 2.32)
	{
		mRemoveSoundsCheckTimer = 0;
		// Find removed sounds and clear references to them
		std::vector<int> removeIds;
		for (std::unordered_map<int, int>::iterator it = mSoundsLogicToVisual.begin(); it != mSoundsLogicToVisual.end();)
		{
			int logicId = it->first;
			int visualId = it->second;
			++it;
			if (!mSoundMgr->SoundExists(logicId))
			{
				mSoundsLogicToVisual.erase(visualId);
				mSoundsVisualToLogic.erase(logicId);
				mSoundsToObjects.erase(logicId);
				removeIds.push_back(visualId);
			}
		}

		// Sync to logic
		if (!removeIds.empty())
			EventManager::Get()->TriggerEvent(std::make_shared<EventDataRemoveSounds>(removeIds));
	}
}

//
// QuakeAIAnalyzerView::OnAttach				- Chapter 19, page 731
//
void QuakeAIAnalyzerView::OnAttach(GameViewId vid, ActorId aid)
{
	HumanView::OnAttach(vid, aid);
}

bool QuakeAIAnalyzerView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
	if (!HumanView::LoadGameDelegate(pLevelData))
		return false;

    mUI.reset(new QuakeAIAnalyzerUI(this));
	mUI->OnInit();

    PushElement(mUI);

	Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));
	Renderer::Get()->ClearBuffers();

	mUI->ShowOverlayMessage(L"Loading...", 0, 0);

	Renderer::Get()->DisplayColorBuffer(0);

	//Create View
	//mUI->ShowOverlayMessage(L"Creating view...", mTextureSrc, 0, 10);

	// Pre-calculated values
	std::shared_ptr<Texture2> texture;
	if (FileSystem::Get()->ExistFile(ToWideString("crack_anylength.png")))
	{
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString("crack_anylength.png")));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			if (resData)
			{
				texture = resData->GetImage();
				texture->AutogenerateMipmaps();
			}
		}
	}
	
	if (texture)
		mCrackAnimationLength = texture->GetDimension(1) / texture->GetDimension(0);
	else 
		mCrackAnimationLength = 5;

	// Set window caption
	std::wstring str = L"Quake";
	System::Get()->SetWindowCaption(str.c_str());

	if (Settings::Get()->GetBool("enable_sound"))
	{
		if (!InitSound())
			return false;
	}

	mStats.gameTime = "";
	mStats.dTimeJitter = { 0 };
	mStats.busyTimeJitter = { 0 };

	mUpdateTimes = { 0 };
	mUpdateTimes.lastTime = Timer::GetTime();

	/* Clear the profiler */
	Profiler::GraphValues dummyvalues;
	Profiling->GraphGet(dummyvalues);

	mScreenSize = Vector2<unsigned int>{
		Settings::Get()->GetUInt16("screen_w"), Settings::Get()->GetUInt16("screen_h") };

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataGameInit>());

	if (!GetGameContent())
	{
		LogError("Connection failed for unknown reason");
		return false;
	}

	// Update cached textures, meshes and materials
	AfterContentReceived();

	// A movement controller is going to control the camera, 
	// but it could be constructed with any of the objects you see in this function.
	mCameraController.reset(new CameraController(mCamera, 0, (float)GE_C_HALF_PI, true));
	mKeyboardHandler = mCameraController;
	mMouseHandler = mCameraController;
	mCamera->ClearTarget();

	EventManager::Get()->TriggerEvent(std::make_shared<EventDataGameReady>(GetActorId()));

	mUI->mHud = std::make_shared<Hud>(mScene.get(), mUI.get());

	const std::string& drawMode = Settings::Get()->Get("mode3d");

	// A movement controller is going to control the camera, 
	// but it could be constructed with any of the objects you see in this function.
	mCamera->GetRelativeTransform().SetTranslation(Vector4<float>::Zero());
	mCamera->ClearTarget();

	mScene->OnRestore();
    return true;
}

//  Run
void QuakeAIAnalyzerView::UpdateProfilers(const AIAnalyzer::RunStats& stats, 
	const AIAnalyzer::FpsControl& updateTimes, float dTime)
{
	float profilerPrintInterval = Settings::Get()->GetFloat("profiler_print_interval");
	bool printToLog = true;

	if (profilerPrintInterval == 0)
	{
		printToLog = false;
		profilerPrintInterval = 3;
	}

	if (mProfilerInterval.Step(dTime, profilerPrintInterval))
	{
		if (printToLog)
		{
			std::stringstream infostream;
			infostream << "Profiler:" << std::endl;
			Profiling->Print(infostream);
			LogInformation(infostream.str());
		}

		mUI->UpdateProfiler();
		Profiling->Clear();
	}

	// Update update graphs
	Profiling->GraphAdd("Sleep [ms]", (float)updateTimes.sleepTime);
	Profiling->GraphAdd("FPS", 1.f / dTime);
}

void QuakeAIAnalyzerView::UpdateStats(AIAnalyzer::RunStats* stats, 
	const AIAnalyzer::FpsControl& updateTimes, float dTime)
{
	float jitter;
	AIAnalyzer::Jitter* jp;

	/* Time average and jitter calculation
	 */
	jp = &stats->dTimeJitter;
	jp->avg = jp->avg * 0.96f + dTime * 0.04f;

	jitter = dTime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;

	jp->counter += dTime;

	if (jp->counter > 0.f)
	{
		jp->counter -= 3.f;
		jp->maxSample = jp->max;
		jp->maxFraction = jp->maxSample / (jp->avg + 0.001f);
		jp->max = 0.f;
	}

	/* Busytime average and jitter calculation
	 */
	jp = &stats->busyTimeJitter;
	jp->avg = jp->avg + updateTimes.busyTime * 0.02f;

	jitter = updateTimes.busyTime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;
	if (jitter < jp->min)
		jp->min = jitter;

	jp->counter += dTime;

	if (jp->counter > 0.f)
	{
		jp->counter -= 3.f;
		jp->maxSample = jp->max;
		jp->minSample = jp->min;
		jp->max = 0.f;
		jp->min = 0.f;
	}
}

bool QuakeAIAnalyzerView::GetGameContent()
{
	ClearInput();

	AIAnalyzer::FpsControl fpsControl = { 0 };
	float dTime; // in seconds
	float progress = 25.f;

	fpsControl.lastTime = Timer::GetTime();

	while (System::Get()->OnRun())
	{
		LimitFps(&fpsControl, &dTime);

		// End condition
		if (mMediaReceived)
			break;

		// Display status
		const std::wstring text = L"Loading Media...";
		progress += dTime * 8;
		if (progress > 100.f) progress = 100.f;

		AIAnalyzer::DrawLoadScreen(text, mUI, mVisual, mBlendState, (int)progress);
	}

	return true;
}

void QuakeAIAnalyzerView::AfterContentReceived()
{
	LogInformation("QuakeAIAnalyzerView::AfterContentReceived() started");
	LogAssert(mMediaReceived, "no media received"); // pre-condition

	std::wstring text = L"Loading textures...";

	// Clear cached pre-scaled 2D GUI images, as this cache
	// might have images with the same name but different
	// content from previous sessions.
	mUI->GetSkin()->ClearTextureCache();

	// Rebuild inherited images and recreate textures
	LogInformation("- Rebuilding images and textures");
	AIAnalyzer::DrawLoadScreen(text, mUI, mVisual, mBlendState, 70);

	// Rebuild shaders
	LogInformation("- Rebuilding shaders");
	text = L"Rebuilding shaders...";
	AIAnalyzer::DrawLoadScreen(text, mUI, mVisual, mBlendState, 71);

	// Update node aliases
	LogInformation("- Updating node aliases");
	text = L"Initializing nodes...";
	AIAnalyzer::DrawLoadScreen(text, mUI, mVisual, mBlendState, 72);

	// Update node textures and assign shaders to each tile
	LogInformation("- Updating node textures");
	AIAnalyzer::TextureUpdateArgs textureUpdateArgs;
	textureUpdateArgs.ui = mUI;
	textureUpdateArgs.scene = mScene.get();
	textureUpdateArgs.lastTimeMs = Timer::GetRealTime();
	textureUpdateArgs.lastPercent = 0;
	textureUpdateArgs.visual = mVisual;
	textureUpdateArgs.blendState = mBlendState;
	textureUpdateArgs.textBase = L"Initializing nodes";

	text = L"Done!";
	AIAnalyzer::DrawLoadScreen(text, mUI, mVisual, mBlendState, 100);
	LogInformation("QuakeAIAnalyzerView::afterContentReceived() done");

	mGameState = BGS_RUNNING;
}


/****************************************************************************
 Input handling
 ****************************************************************************/
void QuakeAIAnalyzerView::ProcessUserInput(float dTime)
{
	// Reset input if window not active or some menu is active
	if (mUI->IsMenuActive() ||
		mUI->HasFocus(mUI->mChatConsole) ||
		mUI->HasFocus(mUI->mForm, true) ||
		!System::Get()->IsWindowActive())
	{
		ClearInput();
	}

	if (!mUI->HasFocus(mUI->mChatConsole) && mUI->mChatConsole->IsOpen())
		mUI->mChatConsole->CloseConsoleAtOnce();

	ProcessKeyInput();
}

void QuakeAIAnalyzerView::ProcessKeyInput()
{
	if (CancelPressed())
	{
		if (!mUI->IsMenuActive() && !mUI->mChatConsole->IsOpenInhibited())
			ShowPauseMenu();
	}
	else if (WasKeyDown(KeyType::SLOT_6))
	{
		QuakeLogic * twg = static_cast<QuakeLogic*>(GameLogic::Get());
		twg->ToggleRenderDiagnostics();
	}
	else if (WasKeyDown(KeyType::SLOT_7))
	{
		mDebugMode = mDebugMode ? DM_OFF : DM_WIREFRAME;
		for (auto const& child : mScene->GetRootNode()->GetChildren())
			child->SetDebugState(mDebugMode);
	}
	else if (WasKeyDown(KeyType::SLOT_8))
	{
		if (mPlayer)
		{
			const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
			for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
			{
				std::shared_ptr<BaseGameView> pView = *it;
				if (pView->GetType() == GV_HUMAN)
				{
					std::shared_ptr<QuakeAIAnalyzerView> pHumanView =
						std::static_pointer_cast<QuakeAIAnalyzerView, BaseGameView>(pView);
					if (pHumanView->GetActorId() != mPlayer->GetId())
					{
						QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
						if (game->GetGameSpec().mModding)
						{
							mPlayer = mScene->GetSceneNode(pHumanView->GetActorId());
							if (mPlayer)
							{
								if (mPlayerController)
									mPlayerController->SetEnabled(false);
								if (mCameraController)
									mCameraController->SetEnabled(false);

								mKeyboardHandler = NULL;
								mMouseHandler = NULL;
								mCamera->SetTarget(mPlayer);

								EventManager::Get()->QueueEvent(
									std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
							}
						}
						else
						{
							mPlayer = mScene->GetSceneNode(pHumanView->GetActorId());
							if (mPlayer)
							{
								if (mPlayerController)
									mPlayerController->SetEnabled(true);
								if (mCameraController)
									mCameraController->SetEnabled(false);

								mKeyboardHandler = mPlayerController;
								mMouseHandler = mPlayerController;
								mCamera->SetTarget(mPlayer);

								EventManager::Get()->QueueEvent(
									std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
							}
						}
						break;
					}
				}
				else if (pView->GetType() == GV_REMOTE)
				{
					std::shared_ptr<NetworkGameView> pNetworkGameView =
						std::static_pointer_cast<NetworkGameView, BaseGameView>(pView);
					if (pNetworkGameView->GetActorId() != mPlayer->GetId())
					{
						mPlayer = mScene->GetSceneNode(pNetworkGameView->GetActorId());
						if (mPlayer)
						{
							if (mPlayerController)
								mPlayerController->SetEnabled(false);
							if (mCameraController)
								mCameraController->SetEnabled(false);

							mKeyboardHandler = NULL;
							mMouseHandler = NULL;
							mCamera->SetTarget(mPlayer);

							EventManager::Get()->QueueEvent(
								std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
						}
						break;
					}
				}
				else if (pView->GetType() == GV_AI)
				{
					std::shared_ptr<QuakeAIView> pAiView =
						std::static_pointer_cast<QuakeAIView, BaseGameView>(pView);
					if (pAiView->GetActorId() != mPlayer->GetId())
					{
						mPlayer = mScene->GetSceneNode(pAiView->GetActorId());
						if (mPlayer)
						{
							if (mPlayerController)
								mPlayerController->SetEnabled(false);
							if (mCameraController)
								mCameraController->SetEnabled(false);

							mKeyboardHandler = NULL;
							mMouseHandler = NULL;
							mCamera->SetTarget(mPlayer);

							EventManager::Get()->QueueEvent(
								std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
						}
						break;
					}
				}
			}
		}
		else
		{
			QuakeLogic* game = static_cast<QuakeLogic*>(GameLogic::Get());
			if (game->GetGameSpec().mModding)
			{
				mPlayer = mScene->GetSceneNode(mActorId);
				if (mPlayer)
				{
					if (mPlayerController)
						mPlayerController->SetEnabled(false);
					if (mCameraController)
						mCameraController->SetEnabled(false);

					mKeyboardHandler = NULL;
					mMouseHandler = NULL;
					mCamera->SetTarget(mPlayer);

					EventManager::Get()->QueueEvent(
						std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
				}
			}
			else
			{
				SetControlledActor(mActorId);

				if (mPlayerController)
					mPlayerController->SetEnabled(true);
				if (mCameraController)
					mCameraController->SetEnabled(false);

				mKeyboardHandler = mPlayerController;
				mMouseHandler = mPlayerController;
				mCamera->SetTarget(mPlayer);

				EventManager::Get()->QueueEvent(
					std::make_shared<EventDataSetControlledActor>(mPlayer->GetId()));
			}
		}
	}
	else if (WasKeyDown(KeyType::SLOT_9))
	{
		if (mPlayerController)
			mPlayerController->SetEnabled(false);
		if (mCameraController)
			mCameraController->SetEnabled(true);

		mKeyboardHandler = mCameraController;
		mMouseHandler = mCameraController;
		mCamera->ClearTarget();

		if (mPlayer)
		{
			EventManager::Get()->QueueEvent(
				std::make_shared<EventDataRemoveControlledActor>(mPlayer->GetId()));
		}
	}
	else if (WasKeyDown(KeyType::CHAT))
	{
		OpenConsole(0.2f, L"");
	}
	else if (WasKeyDown(KeyType::CMD))
	{
		OpenConsole(0.2f, L"/");
	}
	else if (WasKeyDown(KeyType::CMD_LOCAL))
	{
		mUI->ShowStatusText(L"Visual side scripting is disabled");
	}
	else if (WasKeyDown(KeyType::CONSOLE))
	{
		OpenConsole(std::clamp(Settings::Get()->GetFloat("console_height"), 0.1f, 1.0f));
	}
	else if (WasKeyDown(KeyType::PITCHMOVE))
	{
		TogglePitchMove();
	}
	else if (WasKeyDown(KeyType::FASTMOVE))
	{
		ToggleFast();
	}
	else if (WasKeyDown(KeyType::NOCLIP))
	{
		ToggleNoClip();
	}
	else if (WasKeyDown(KeyType::MUTE))
	{
		if (Settings::Get()->GetBool("enable_sound"))
		{
			bool newMuteSound = !Settings::Get()->GetBool("mute_sound");
			Settings::Get()->SetBool("mute_sound", newMuteSound);
			if (newMuteSound)
				mUI->ShowTranslatedStatusText("Sound muted");
			else
				mUI->ShowTranslatedStatusText("Sound unmuted");
		}
		else mUI->ShowTranslatedStatusText("Sound system is disabled");
	}
	else if (WasKeyDown(KeyType::INC_VOLUME))
	{
		if (Settings::Get()->GetBool("enable_sound"))
		{
			float newVolume = std::clamp(Settings::Get()->GetFloat("sound_volume") + 0.1f, 0.0f, 1.0f);
			wchar_t buf[100];
			Settings::Get()->SetFloat("sound_volume", newVolume);
			const wchar_t* str = L"Volume changed to %d%%";
			swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, round(newVolume * 100));
			delete[] str;
			mUI->ShowStatusText(buf);
		}
		else mUI->ShowTranslatedStatusText("Sound system is disabled");
	}
	else if (WasKeyDown(KeyType::DEC_VOLUME))
	{
		if (Settings::Get()->GetBool("enable_sound"))
		{
			float newVolume = std::clamp(Settings::Get()->GetFloat("sound_volume") - 0.1f, 0.0f, 1.0f);
			wchar_t buf[100];
			Settings::Get()->SetFloat("sound_volume", newVolume);
			const wchar_t* str = L"Volume changed to %d%%";
			swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, round(newVolume * 100));
			delete[] str;
			mUI->ShowStatusText(buf);
		}
		else mUI->ShowTranslatedStatusText("Sound system is disabled");
	}
	else if (WasKeyDown(KeyType::CINEMATIC))
	{
		ToggleCinematic();
	}
	else if (WasKeyDown(KeyType::TOGGLE_HUD))
	{
		mUI->ToggleHud();
	}
	else if (WasKeyDown(KeyType::TOGGLE_CHAT))
	{
		mUI->ToggleChat();
	}
	else if (WasKeyDown(KeyType::TOGGLE_FOG))
	{
		ToggleFog();
	}
	else if (WasKeyDown(KeyType::TOGGLE_DEBUG))
	{
		ToggleDebug();
	}
	else if (WasKeyDown(KeyType::TOGGLE_PROFILER))
	{
		mUI->ToggleProfiler();
	}
	else if (WasKeyDown(KeyType::INCREASE_VIEWING_RANGE))
	{
		IncreaseViewRange();
	}
	else if (WasKeyDown(KeyType::DECREASE_VIEWING_RANGE))
	{
		DecreaseViewRange();
	}
	else if (WasKeyDown(KeyType::RANGESELECT))
	{
		ToggleFullViewRange();
	}
	else if (WasKeyDown(KeyType::ZOOM))
	{
		CheckZoomEnabled();
	}
}

void QuakeAIAnalyzerView::UpdateFrame(AIAnalyzer::RunStats* stats, float dTime)
{
	TimeTaker ttUpdate("UpdateFrame");

	mUI->Update(*stats, mCamera, mUI->mChatConsole, dTime);

	/*
	   make sure menu is on top
	   1. Delete form menu reference if menu was removed
	   2. Else, make sure form menu is on top
	*/
	do
	{
		// breakable. only runs for one iteration
		if (!mUI->GetForm())
			break;

		if (!mUI->GetForm()->IsActive())
		{
			mUI->DeleteForm();
			break;
		}

		std::shared_ptr<UIForm>& formUI =
			std::static_pointer_cast<UIForm>(mUI->GetForm());
		//formUI->QuitForm();

		if (mUI->IsMenuActive())
			mUI->GetRootUIElement()->BringToFront(formUI);

	} while (false);

	Profiling->GraphAdd("Update frame [ms]", (float)ttUpdate.Stop(true));
}


bool QuakeAIAnalyzerView::LoadMedia(const std::string& filePath, bool fromMediaPush)
{
	std::string name;

	const char* imageExt[] = {
		".png", ".jpg", ".bmp", ".tga",
		".pcx", ".ppm", ".psd", ".wal", ".rgb", NULL };
	name = StringRemoveEnd(filePath, imageExt);
	if (!name.empty())
	{
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(filePath)));
		if (resHandle)
		{
			std::shared_ptr<ImageResourceExtraData> resData =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			auto fileName = FileSystem::Get()->GetFileName(ToWideString(filePath));
			std::shared_ptr<Texture2> texture = resData->GetImage();
			texture->SetName(fileName);
			texture->AutogenerateMipmaps();

			return true;
		}

		LogWarning("Couldn't load image file \"" + filePath + "\"");
		return false;
	}

	const char* soundExt[] = {".wav", ".ogg", NULL };
	name = StringRemoveEnd(filePath, soundExt);
	if (!name.empty())
	{
		auto fileName = FileSystem::Get()->GetFileName(ToWideString(name));
		if (mSoundMgr->LoadSound(ToString(fileName.c_str()), filePath))
			return true;

		LogWarning("Couldn't load sound file \"" + filePath + "\"");
		return false;
	}

	const char* modelExt[] = { ".bsp", ".pk3", ".md3", NULL };
	name = StringRemoveEnd(filePath, modelExt);
	if (!name.empty())
	{
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(filePath)));
		if (resHandle)
			return true;

		LogWarning("Couldn't load model into memory : \"" + filePath + "\"");
		return false;
	}

	const char* translate_ext[] = { ".tr", NULL };
	name = StringRemoveEnd(filePath, translate_ext);
	if (!name.empty())
	{
		if (fromMediaPush)
			return false;

		LogInformation("Loading translation: \"" + filePath + "\"");
		//LoadTranslation(data);
		return true;
	}

	LogError("Unrecognized file format to load \"" + filePath + "\"");
	return false;
}

std::string QuakeAIAnalyzerView::GetModStoragePath() const
{
	std::string currentDir = ToString(FileSystem::Get()->GetWorkingDirectory());
	return currentDir + "/mod_storage";
}

void QuakeAIAnalyzerView::SetControlledActor(ActorId actorId)
{
	mPlayer = mScene->GetSceneNode(actorId);
	if (!mPlayer)
	{
		LogError("Invalid player");
		return;
	}

	HumanView::SetControlledActor(actorId);

	AxisAngle<4, float> localRotation;
	mPlayer->GetRelativeTransform().GetRotation(localRotation);
	float yaw = localRotation.mAngle * localRotation.mAxis[AXIS_Y];
	mPlayerController.reset(new QuakePlayerController(mPlayer, yaw, 0.f));

	mKeyboardHandler = mPlayerController;
	mMouseHandler = mPlayerController;
}

void QuakeAIAnalyzerView::ShowFormDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowForm> pCastEventData =
		std::static_pointer_cast<EventDataShowForm>(pEventData);

	std::string showForm = pCastEventData->GetForm();
	std::string showFormName = pCastEventData->GetFormName();

	if (!showForm.empty())
	{
		std::string formPr;
		std::shared_ptr<FormSource> formSrc(new FormSource(showForm));
		std::shared_ptr<TextDestination> textDst(new TextDestination());

		RectangleShape<2, int> rectangle;
		rectangle.mCenter = Vector2<int>{ 50, 50 };
		rectangle.mExtent = Vector2<int>{ 100, 100 };

		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm(showFormName);
		if (formUI)
		{
			formUI->SetFormPrepend(formPr);
			formUI->SetFormSource(formSrc);
			formUI->SetTextDestination(textDst);
		}
		else
		{
			formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
			formUI->SetParent(mUI->GetRootUIElement());
			formUI->OnInit();
		}
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		if (formUI && showFormName.empty() || showForm == mUI->GetFormName())
			formUI->QuitForm();
	};
}


void QuakeAIAnalyzerView::InitChatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataInitChat> pCastEventData =
		std::static_pointer_cast<EventDataInitChat>(pEventData);

	mUI->mChatConsole->SetChat(pCastEventData->GetChat());
}


void QuakeAIAnalyzerView::UpdateChatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataUpdateChat> pCastEventData =
		std::static_pointer_cast<EventDataUpdateChat>(pEventData);

	// Display all messages in a static text element
	mUI->SetChatText(pCastEventData->GetChat(), pCastEventData->GetLineCount());
}

void QuakeAIAnalyzerView::HandlePlaySoundAtDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPlaySoundAt> pCastEventData =
		std::static_pointer_cast<EventDataPlaySoundAt>(pEventData);

	mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
		pCastEventData->GetGain(), pCastEventData->GetPosition(), pCastEventData->GetPitch());
}

void QuakeAIAnalyzerView::HandlePlaySoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPlaySoundType> pCastEventData =
		std::static_pointer_cast<EventDataPlaySoundType>(pEventData);

	// Start playing
	int soundId = -1;
	Vector3<float> pos = pCastEventData->GetPosition();
	switch (pCastEventData->GetType())
	{
		case 0: // local
			soundId = mSoundMgr->PlaySoundGlobal(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
				pCastEventData->GetGain(), pCastEventData->GetFade(), pCastEventData->GetPitch());
			break;
		case 1: // positional
			soundId = mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
				pCastEventData->GetGain(), pCastEventData->GetPosition(), pCastEventData->GetPitch());
			break;
		case 2:
		{
			// object
			std::shared_ptr<Actor> pActor = GameLogic::Get()->GetActor(pCastEventData->GetObjectId()).lock();
			if (pActor)
			{
				std::shared_ptr<TransformComponent> pTransformComponent(
					pActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
				if (pTransformComponent)
					pos = pTransformComponent->GetPosition();
			}

			soundId = mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
				pCastEventData->GetGain(), pos, pCastEventData->GetPitch());
			break;
		}
		default:
			break;
	}

	if (soundId != -1)
	{
		// for ephemeral sounds, id is not meaningful
		if (!pCastEventData->IsEphemeral())
		{
			mSoundsLogicToVisual[pCastEventData->GetId()] = soundId;
			mSoundsVisualToLogic[soundId] = pCastEventData->GetId();
		}
		if (pCastEventData->GetObjectId() != 0)
			mSoundsToObjects[soundId] = pCastEventData->GetObjectId();
	}
}

void QuakeAIAnalyzerView::HandleStopSoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataStopSound> pCastEventData =
		std::static_pointer_cast<EventDataStopSound>(pEventData);

	std::unordered_map<int, int>::iterator itSound = mSoundsLogicToVisual.find(pCastEventData->GetId());
	if (itSound != mSoundsLogicToVisual.end())
	{
		int soundId = itSound->second;
		mSoundMgr->StopSound(soundId);
	}
}

void QuakeAIAnalyzerView::HandleFadeSoundDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFadeSound> pCastEventData =
		std::static_pointer_cast<EventDataFadeSound>(pEventData);

	std::unordered_map<int, int>::const_iterator itSound = mSoundsLogicToVisual.find(pCastEventData->GetId());
	if (itSound != mSoundsLogicToVisual.end())
	{
		int soundId = itSound->second;
		mSoundMgr->FadeSound(soundId, pCastEventData->GetStep(), pCastEventData->GetGain());
	}
}

void QuakeAIAnalyzerView::ChangeVolumeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeVolume> pCastEventData =
		std::static_pointer_cast<EventDataChangeVolume>(pEventData);

	RectangleShape<2, int> rect;
	rect.mExtent = Vector2<int>{ 100, 100 };
	rect.mCenter = rect.mExtent / 2;

	std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
	formUI.reset( new UIVolumeChange(mUI.get(), -1, rect));
	formUI->SetParent(mUI->GetRootUIElement());
	formUI->OnInit();
}

void QuakeAIAnalyzerView::HandleMediaDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataHandleMedia> pCastEventData =
		std::static_pointer_cast<EventDataHandleMedia>(pEventData);

	// Check media cache
	for (auto const& media : pCastEventData->GetMedia())
		LoadMedia(ToString(media.second));

	mMediaReceived = true;
}

void QuakeAIAnalyzerView::ChangeMenuDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeMenu> pCastEventData =
		std::static_pointer_cast<EventDataChangeMenu>(pEventData);

	RectangleShape<2, int> rect;
	rect.mExtent = Vector2<int>{ 100, 100 };
	rect.mCenter = rect.mExtent / 2;

	std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
	formUI.reset(new UIKeyChange(mUI.get(), -1, rect));
	formUI->SetParent(mUI->GetRootUIElement());
	formUI->OnInit();
}

void QuakeAIAnalyzerView::GameplayUiUpdateDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGameplayUIUpdate> pCastEventData =
		std::static_pointer_cast<EventDataGameplayUIUpdate>(pEventData);
    if (!pCastEventData->GetUiString().empty())
        mGameplayText = pCastEventData->GetUiString();
    else
		mGameplayText.clear();
}

void QuakeAIAnalyzerView::FireWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFireWeapon> pCastEventData =
		std::static_pointer_cast<EventDataFireWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim ||
					mesh->GetCurrentFrame() == mesh->GetAnimation(torsoAnim).mEndFrame)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeAIAnalyzerView::ChangeWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeWeapon> pCastEventData =
		std::static_pointer_cast<EventDataChangeWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);

		int weaponIdx = 0;
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetParent() && mesh->GetParent()->GetName() == "tag_weapon")
			{
				weaponIdx++;
				if (pPlayerActor->GetState().weapon == weaponIdx)
					mesh->SetRenderMesh(true);
				else
					mesh->SetRenderMesh(false);
			}

			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeAIAnalyzerView::DeadActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataDeadActor> pCastEventData =
		std::static_pointer_cast<EventDataDeadActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);

		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetParent() && mesh->GetParent()->GetName() == "tag_weapon")
				mesh->SetRenderMesh(false);

			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				mesh->SetCurrentAnimation(legsAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				mesh->SetCurrentAnimation(torsoAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
			}
		}
	}
}

void QuakeAIAnalyzerView::SpawnActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSpawnActor> pCastEventData =
		std::static_pointer_cast<EventDataSpawnActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);

		int weaponIdx = 0;
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetParent() && mesh->GetParent()->GetName() == "tag_weapon")
			{
				weaponIdx++;
				if (pPlayerActor->GetState().weapon == weaponIdx)
					mesh->SetRenderMesh(true);
				else
					mesh->SetRenderMesh(false);
			}

			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				mesh->SetCurrentAnimation(legsAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				mesh->SetCurrentAnimation(torsoAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
			}
		}
	}

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);
}

void QuakeAIAnalyzerView::JumpActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataJumpActor> pCastEventData =
		std::static_pointer_cast<EventDataJumpActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				mesh->SetCurrentAnimation(legsAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				mesh->SetCurrentAnimation(torsoAnim);
				mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
			}
		}
	}
}

void QuakeAIAnalyzerView::MoveActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataMoveActor> pCastEventData =
		std::static_pointer_cast<EventDataMoveActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	if (pPlayerActor->GetState().weaponState != WEAPON_READY)
		return;

	std::shared_ptr<PhysicComponent> pPhysicComponent(
		pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
	if (pPhysicComponent)
		if (!pPhysicComponent->OnGround())
			return;

	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode = 
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeAIAnalyzerView::FallActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFallActor> pCastEventData =
		std::static_pointer_cast<EventDataFallActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(actorId).lock()));
	if (pPlayerActor->GetState().weaponState != WEAPON_READY)
		return;

	std::shared_ptr<PhysicComponent> pPhysicComponent(
		pPlayerActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
	if (pPhysicComponent)
		if (!pPhysicComponent->OnGround())
			return;

	std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
	if (pNode)
	{
		std::shared_ptr<AnimatedMeshNode> animatedNode =
			std::dynamic_pointer_cast<AnimatedMeshNode>(pNode);
		std::shared_ptr<AnimateMeshMD3> animMeshMD3 =
			std::dynamic_pointer_cast<AnimateMeshMD3>(animatedNode->GetMesh());

		std::vector<std::shared_ptr<MD3Mesh>> meshes;
		animMeshMD3->GetMD3Mesh()->GetMeshes(meshes);
		for (std::shared_ptr<MD3Mesh> mesh : meshes)
		{
			if (mesh->GetName() == "lower")
			{
				//run animation
				int legsAnim = pPlayerActor->GetState().legsAnim;
				if (mesh->GetCurrentAnimation() != legsAnim)
				{
					mesh->SetCurrentAnimation(legsAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(legsAnim).mBeginFrame);
				}
			}
			else if (mesh->GetName() == "upper")
			{
				//run animation
				int torsoAnim = pPlayerActor->GetState().torsoAnim;
				if (mesh->GetCurrentAnimation() != torsoAnim)
				{
					mesh->SetCurrentAnimation(torsoAnim);
					mesh->SetCurrentFrame((float)mesh->GetAnimation(torsoAnim).mBeginFrame);
				}
			}
		}
	}
}

void QuakeAIAnalyzerView::RotateActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataRotateActor> pCastEventData =
		std::static_pointer_cast<EventDataRotateActor>(pEventData);

	std::shared_ptr<Actor> pGameActor(
		GameLogic::Get()->GetActor(pCastEventData->GetId()).lock());
	if (pGameActor)
	{
		ActorId actorId = pCastEventData->GetId();
		std::shared_ptr<Node> pNode = mScene->GetSceneNode(actorId);
		if (pNode)
		{
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), pCastEventData->GetYaw() * (float)GE_C_DEG_TO_RAD));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), pCastEventData->GetPitch() * (float)GE_C_DEG_TO_RAD));

			pNode->GetRelativeTransform().SetRotation(yawRotation * pitchRotation);
		}
	}
}

void QuakeAIAnalyzerView::ChangeAnalysisFrameDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeAnalysisFrame> pCastEventData =
		std::static_pointer_cast<EventDataChangeAnalysisFrame>(pEventData);

	const std::shared_ptr<UIForm>& form = std::static_pointer_cast<UIForm>(mUI->GetForm());
	const std::shared_ptr<BaseUIElement>& frameInput =
		std::static_pointer_cast<BaseUIElement>(form->GetElementFromId(form->GetField("te_search")));
	frameInput->SetText(std::to_wstring(pCastEventData->GetFrame()).c_str());
}

void QuakeAIAnalyzerView::ShowGameSimulationDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowGameSimulation> pCastEventData =
		std::static_pointer_cast<EventDataShowGameSimulation>(pEventData);

	const std::shared_ptr<UIForm>& form = std::static_pointer_cast<UIForm>(mUI->GetForm());
	const std::shared_ptr<BaseUIElement>& searchElement =
		std::static_pointer_cast<BaseUIElement>(form->GetElementFromId(form->GetField("te_search")));
	searchElement->SetText(std::to_wstring(pCastEventData->GetFrame()).c_str());

	std::shared_ptr<BaseUIScrollBar>& scrollbar =
		std::static_pointer_cast<BaseUIScrollBar>(form->GetElementFromId(form->GetField("scrbar")));
	scrollbar->SetPosition(pCastEventData->GetFrame());

	if (!mGameAISimulation)
	{
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		mGameAIState = aiManager->GetGame().states[pCastEventData->GetFrame()];
		for (AIGame::Item item : mGameAIState.items)
		{
			std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.id);
			pItemNode->SetVisible(item.visible);
		}

		UpdateGameAIState();
	}
	else
	{
		UpdateGameAISimulation(pCastEventData->GetFrame());
	}
}

void QuakeAIAnalyzerView::ShowGameStateDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowGameState> pCastEventData =
		std::static_pointer_cast<EventDataShowGameState>(pEventData);

	const std::shared_ptr<UIForm>& form = std::static_pointer_cast<UIForm>(mUI->GetForm());
	const std::shared_ptr<BaseUIElement>& searchElement =
		std::static_pointer_cast<BaseUIElement>(form->GetElementFromId(form->GetField("te_search")));
	searchElement->SetText(std::to_wstring(pCastEventData->GetFrame()).c_str());

	std::shared_ptr<BaseUIScrollBar>& scrollbar = 
		std::static_pointer_cast<BaseUIScrollBar>(form->GetElementFromId(form->GetField("scrbar")));
	scrollbar->SetPosition(pCastEventData->GetFrame());

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	mGameAIState = aiManager->GetGame().states[pCastEventData->GetFrame()];
	UpdateGameAIState();

	AIGame::GameState gameState = aiManager->GetGame().states[pCastEventData->GetFrame()];
	for (AIGame::Item item : gameState.items)
	{
		std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.id);
		pItemNode->SetVisible(item.visible);
	}
}

void QuakeAIAnalyzerView::SimulateAIGameDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSimulateAIGame> pCastEventData =
		std::static_pointer_cast<EventDataSimulateAIGame>(pEventData);

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
		if (pAiView)
		{
			//pAiView->SetBehavior(BT_PATROL);
			pAiView->SetPathingGraph(aiManager->GetPathingGraph());
			pAiView->SetEnabled(false);
		}
	}
}

void QuakeAIAnalyzerView::AnalyzeAIGameDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataAnalyzeAIGame> pCastEventData =
		std::static_pointer_cast<EventDataAnalyzeAIGame>(pEventData);

	//show analysis data
	mUI->mFlags.showAnalysis = true;

	AnalyzeAIGame(pCastEventData->GetTab(), pCastEventData->GetGameFrame(), pCastEventData->GetAnalysisFrame(), 
		pCastEventData->GetPlayer(), pCastEventData->GetDecisionCluster(), pCastEventData->GetEvaluationCluster(), 
		pCastEventData->GetDecisionFilter(), pCastEventData->GetEvaluationFilter());

	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
		if (pAiView)
			pAiView->SetEnabled(false);
	}
}

void QuakeAIAnalyzerView::ShowAIGameAnalysisDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowAIGameAnalysis> pCastEventData =
		std::static_pointer_cast<EventDataShowAIGameAnalysis>(pEventData);

	if (pCastEventData->GetTab() == 1)
	{
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		mGameAIState = aiManager->GetGame().states[pCastEventData->GetGameFrame()];
		for (AIGame::Item item : mGameAIState.items)
		{
			std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.id);
			pItemNode->SetVisible(item.visible);
		}

		mGameAISimulation = false;
		UpdateGameAIState();
	}
	else
	{
		mGameAISimulation = true;
		UpdateGameAIAnalysis(pCastEventData->GetTab(), pCastEventData->GetAnalysisFrame());
	}

	ShowAIGameAnalysis(pCastEventData->GetTab(), pCastEventData->GetGameFrame(), pCastEventData->GetAnalysisFrame(), 
		pCastEventData->GetPlayer(), pCastEventData->GetDecisionCluster(), pCastEventData->GetEvaluationCluster(), 
		pCastEventData->GetDecisionFilter(), pCastEventData->GetEvaluationFilter());

	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
		if (pAiView)
			pAiView->SetEnabled(false);
	}
}

void QuakeAIAnalyzerView::ShowAIGameDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowAIGame> pCastEventData =
		std::static_pointer_cast<EventDataShowAIGame>(pEventData);

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	ShowAIGame(aiManager->GetGame());

	mGameAIState = aiManager->GetGame().states.front();
	UpdateGameAIState();

	const GameViewList& gameViews = GameApplication::Get()->GetGameViews();
	for (auto it = gameViews.begin(); it != gameViews.end(); ++it)
	{
		std::shared_ptr<QuakeAIView> pAiView = std::dynamic_pointer_cast<QuakeAIView>(*it);
		if (pAiView)
			pAiView->SetEnabled(false);
	}
}

void QuakeAIAnalyzerView::RegisterAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::InitChatDelegate),
		EventDataInitChat::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::UpdateChatDelegate),
		EventDataUpdateChat::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowFormDelegate),
		EventDataShowForm::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandlePlaySoundAtDelegate),
		EventDataPlaySoundAt::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandlePlaySoundDelegate),
		EventDataPlaySoundType::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandleStopSoundDelegate),
		EventDataStopSound::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandleFadeSoundDelegate),
		EventDataFadeSound::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeVolumeDelegate),
		EventDataChangeVolume::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeMenuDelegate),
		EventDataChangeMenu::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandleMediaDelegate),
		EventDataHandleMedia::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::FireWeaponDelegate),
		EventDataFireWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeWeaponDelegate),
		EventDataChangeWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::DeadActorDelegate),
		EventDataDeadActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::SpawnActorDelegate),
		EventDataSpawnActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::MoveActorDelegate),
		EventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::FallActorDelegate),
		EventDataFallActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::RotateActorDelegate),
		EventDataRotateActor::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeAnalysisFrameDelegate),
		EventDataChangeAnalysisFrame::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowGameSimulationDelegate),
		EventDataShowGameSimulation::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowGameStateDelegate),
		EventDataShowGameState::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::SimulateAIGameDelegate),
		EventDataSimulateAIGame::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::AnalyzeAIGameDelegate),
		EventDataAnalyzeAIGame::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowAIGameDelegate),
		EventDataShowAIGame::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowAIGameAnalysisDelegate),
		EventDataShowAIGameAnalysis::skEventType);
}

void QuakeAIAnalyzerView::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::InitChatDelegate),
		EventDataInitChat::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::UpdateChatDelegate),
		EventDataUpdateChat::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowFormDelegate),
		EventDataShowForm::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandlePlaySoundAtDelegate),
		EventDataPlaySoundAt::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandlePlaySoundDelegate),
		EventDataPlaySoundType::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandleStopSoundDelegate),
		EventDataStopSound::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandleFadeSoundDelegate),
		EventDataFadeSound::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeVolumeDelegate),
		EventDataChangeVolume::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeMenuDelegate),
		EventDataChangeMenu::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::HandleMediaDelegate),
		EventDataHandleMedia::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::FireWeaponDelegate),
		EventDataFireWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeWeaponDelegate),
		EventDataChangeWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::DeadActorDelegate),
		EventDataDeadActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::SpawnActorDelegate),
		EventDataSpawnActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::MoveActorDelegate),
		EventDataMoveActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::FallActorDelegate),
		EventDataFallActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::RotateActorDelegate),
		EventDataRotateActor::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ChangeAnalysisFrameDelegate),
		EventDataChangeAnalysisFrame::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowGameSimulationDelegate),
		EventDataShowGameSimulation::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowGameStateDelegate),
		EventDataShowGameState::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::SimulateAIGameDelegate),
		EventDataSimulateAIGame::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::AnalyzeAIGameDelegate),
		EventDataAnalyzeAIGame::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowAIGameDelegate),
		EventDataShowAIGame::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIAnalyzerView::ShowAIGameAnalysisDelegate),
		EventDataShowAIGameAnalysis::skEventType);
}

void QuakeAIAnalyzerView::OpenConsole(float scale, const wchar_t* line)
{
	LogAssert(scale > 0.0f && scale <= 1.0f, "invalid scale");

	if (mUI->mChatConsole->IsOpenInhibited())
		return;

	mUI->mChatConsole->OpenConsole(scale);
	if (line)
	{
		mUI->mChatConsole->SetCloseOnEnter(true);
		mUI->mChatConsole->ReplaceAndAddToHistory(line);
	}
}

void QuakeAIAnalyzerView::ToggleFreeMove()
{
	bool freeMove = !Settings::Get()->GetBool("free_move");
	Settings::Get()->Set("free_move", freeMove ? "true" : "false");

	if (freeMove)
	{
		const bool hasFlyPrivs = false;// mEnvironment->CheckPrivilege("fly");
		if (hasFlyPrivs)
			mUI->ShowTranslatedStatusText("Fly mode enabled");
		else
			mUI->ShowTranslatedStatusText("Fly mode enabled (note: no 'fly' privilege)");
	}
	else
	{
		mUI->ShowTranslatedStatusText("Fly mode disabled");
	}
}

void QuakeAIAnalyzerView::ToggleFreeMoveAlt()
{
	if (mSettings.doubletapJump)
		ToggleFreeMove();
}

void QuakeAIAnalyzerView::TogglePitchMove()
{
	bool pitchMove = !Settings::Get()->GetBool("pitch_move");
	Settings::Get()->Set("pitch_move", pitchMove ? "true" : "false");

	if (pitchMove)
		mUI->ShowTranslatedStatusText("Pitch move mode enabled");
	else
		mUI->ShowTranslatedStatusText("Pitch move mode disabled");
}

void QuakeAIAnalyzerView::ToggleFast()
{
	bool fastMove = !Settings::Get()->GetBool("fast_move");
	Settings::Get()->Set("fast_move", fastMove ? "true" : "false");

	if (fastMove)
	{
		const bool hasFastPrivs = false; //mEnvironment->CheckPrivilege("fast");
		if (hasFastPrivs)
			mUI->ShowTranslatedStatusText("Fast mode enabled");
		else
			mUI->ShowTranslatedStatusText("Fast mode enabled (note: no 'fast' privilege)");
	}
	else
		mUI->ShowTranslatedStatusText("Fast mode disabled");
}

void QuakeAIAnalyzerView::ToggleNoClip()
{
	bool noClip = !Settings::Get()->GetBool("noclip");
	Settings::Get()->Set("noclip", noClip ? "true" : "false");

	if (noClip)
	{
		const bool hasNoClipPrivs = false; //mEnvironment->CheckPrivilege("noclip");
		if (hasNoClipPrivs)
			mUI->ShowTranslatedStatusText("Noclip mode enabled");
		else
			mUI->ShowTranslatedStatusText("Noclip mode enabled (note: no 'noClip' privilege)");
	}
	else
		mUI->ShowTranslatedStatusText("Noclip mode disabled");
}

void QuakeAIAnalyzerView::ToggleCinematic()
{
	bool cinematic = !Settings::Get()->GetBool("cinematic");
	Settings::Get()->Set("cinematic", cinematic ? "true" : "false");

	if (cinematic)
		mUI->ShowTranslatedStatusText("Cinematic mode enabled");
	else
		mUI->ShowTranslatedStatusText("Cinematic mode disabled");
}

// Autoforward by toggling continuous forward.
void QuakeAIAnalyzerView::ToggleAutoforward()
{
	bool autorunEnabled = !Settings::Get()->GetBool("continuous_forward");
	Settings::Get()->Set("continuous_forward", autorunEnabled ? "true" : "false");

	if (autorunEnabled)
		mUI->ShowTranslatedStatusText("Automatic forward enabled");
	else
		mUI->ShowTranslatedStatusText("Automatic forward disabled");
}

void QuakeAIAnalyzerView::ToggleFog()
{
	bool fogEnabled = Settings::Get()->GetBool("enable_fog");
	Settings::Get()->SetBool("enable_fog", !fogEnabled);
	if (fogEnabled)
		mUI->ShowTranslatedStatusText("Fog disabled");
	else
		mUI->ShowTranslatedStatusText("Fog enabled");
}

void QuakeAIAnalyzerView::ToggleDebug()
{
	const bool hasDebugPrivs = true; //mEnvironment->CheckPrivilege("debug");

	// Initial / 4x toggle: Chat only
	// 1x toggle: Debug text with chat
	// 2x toggle: Debug text with profiler graph
	// 3x toggle: Debug text and wireframe
	if (!mUI->mFlags.showDebug)
	{
		mUI->mFlags.showDebug = true;
		mUI->mFlags.showProfilerGraph = false;
		mUI->ShowTranslatedStatusText("Debug info shown");
	}
	else if (!mUI->mFlags.showProfilerGraph)
	{
		mUI->mFlags.showProfilerGraph = true;
		mUI->ShowTranslatedStatusText("Profiler graph shown");
	}
	else if (hasDebugPrivs)
	{
		mUI->mFlags.showProfilerGraph = false;
		mUI->ShowTranslatedStatusText("Wireframe shown");
	}
	else
	{
		mUI->mFlags.showDebug = false;
		mUI->mFlags.showProfilerGraph = false;
		if (hasDebugPrivs)
			mUI->ShowTranslatedStatusText("Debug info, profiler graph, and wireframe hidden");
		else
			mUI->ShowTranslatedStatusText("Debug info and profiler graph hidden");
	}
}

void QuakeAIAnalyzerView::IncreaseViewRange()
{
	int16_t range = Settings::Get()->GetInt16("viewing_range");
	int16_t rangeNew = range + 10;

	wchar_t buf[255];
	const wchar_t* str;
	if (rangeNew > 4000)
	{
		rangeNew = 4000;
		str = L"Viewing range is at maximum: %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mUI->ShowStatusText(buf);
	}
	else
	{
		str = L"Viewing range changed to %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mUI->ShowStatusText(buf);
	}
	Settings::Get()->Set("viewing_range", std::to_string(rangeNew));
}

void QuakeAIAnalyzerView::DecreaseViewRange()
{
	int16_t range = Settings::Get()->GetInt16("viewing_range");
	int16_t rangeNew = range - 10;

	wchar_t buf[255];
	const wchar_t* str;
	if (rangeNew < 20)
	{
		rangeNew = 20;
		str = L"Viewing range is at minimum: %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mUI->ShowStatusText(buf);
	}
	else
	{
		str = L"Viewing range changed to %d";
		swprintf(buf, sizeof(buf) / sizeof(wchar_t), str, rangeNew);
		delete[] str;
		mUI->ShowStatusText(buf);
	}
	Settings::Get()->Set("viewingRange", std::to_string(rangeNew));
}

void QuakeAIAnalyzerView::ToggleFullViewRange()
{
	mUI->ShowTranslatedStatusText("Disabled unlimited viewing range");
}

void QuakeAIAnalyzerView::CheckZoomEnabled()
{

}


/****************************************************************************/
/****************************************************************************
 Shutdown / cleanup
 ****************************************************************************/
 /****************************************************************************/

void QuakeAIAnalyzerView::ExtendedResourceCleanup()
{
	// Extended resource accounting
	LogInformation("Game resources after cleanup:");
}

void QuakeAIAnalyzerView::Shutdown()
{
	std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
	if (formUI)
		formUI->QuitForm();

	Renderer::Get()->SetClearColor(SColor(255, 0, 0, 0));
	Renderer::Get()->ClearBuffers();

	mUI->ShowOverlayMessage(L"Shutting down...", 0, false);

	Renderer::Get()->DisplayColorBuffer(0);

	/* cleanup menus */
	if (mUI->GetForm())
		mUI->GetForm()->SetVisible(false);
	mUI->DeleteForm();

	Sleep(100);

	ExtendedResourceCleanup();
}

void QuakeAIAnalyzerView::PrintPlayerItems(const std::unordered_map<ActorId, float>& items, std::stringstream& output)
{
	std::vector<std::string> itemsName;
	for (auto const& item : items)
	{
		std::shared_ptr<Actor> pItemActor = GameLogic::Get()->GetActor(item.first).lock();
		if (pItemActor->GetType() == "Weapon")
		{
			std::shared_ptr<WeaponPickup> pWeaponPickup =
				pItemActor->GetComponent<WeaponPickup>(WeaponPickup::Name).lock();
			switch (pWeaponPickup->GetCode())
			{
				case 1:
					itemsName.push_back("weapon shotgun");
					break;
				case 2:
					itemsName.push_back("weapon rocketlauncher");
					break;
				case 3:
					itemsName.push_back("weapon railgun");
					break;
				case 4:
					itemsName.push_back("weapon plasmagun");
					break;
				case 5:
					itemsName.push_back("weapon machinegun");
					break;
				case 6:
					itemsName.push_back("weapon lightning");
					break;
				case 7:
					itemsName.push_back("weapon grenadelauncher");
					break;
				case 8:
					itemsName.push_back("weapon gaunlet");
					break;
			}
		}
		else if (pItemActor->GetType() == "Ammo")
		{
			std::shared_ptr<AmmoPickup> pAmmoPickup =
				pItemActor->GetComponent<AmmoPickup>(AmmoPickup::Name).lock();
			switch (pAmmoPickup->GetCode())
			{
				case 1:
					itemsName.push_back("ammo shell");
					break;
				case 2:
					itemsName.push_back("ammo rocket");
					break;
				case 3:
					itemsName.push_back("ammo slug");
					break;
				case 4:
					itemsName.push_back("ammo cell");
					break;
				case 5:
					itemsName.push_back("ammo bullet");
					break;
				case 6:
					itemsName.push_back("ammo lightning");
					break;
				case 7:
					itemsName.push_back("ammo grenade");
					break;
			}
		}
		else if (pItemActor->GetType() == "Armor")
		{
			std::shared_ptr<ArmorPickup> pArmorPickup =
				pItemActor->GetComponent<ArmorPickup>(ArmorPickup::Name).lock();
			switch (pArmorPickup->GetCode())
			{
				case 1:
					itemsName.push_back("armor body");
					break;
				case 2:
					itemsName.push_back("armor combat");
					break;
				case 3:
					itemsName.push_back("armor shard");
					break;
			}

		}
		else if (pItemActor->GetType() == "Health")
		{
			std::shared_ptr<HealthPickup> pHealthPickup =
				pItemActor->GetComponent<HealthPickup>(HealthPickup::Name).lock();
			switch (pHealthPickup->GetCode())
			{
				case 1:
					itemsName.push_back("health normal");
					break;
				case 2:
					itemsName.push_back("health large");
					break;
				case 3:
					itemsName.push_back("health mega");
					break;
				case 4:
					itemsName.push_back("health small");
					break;
			}
		}
	}

	std::map<std::string, unsigned int> itemsCount;
	for (std::string itemName : itemsName)
	{
		if (itemsCount.find(itemName) == itemsCount.end())
			itemsCount[itemName] = 0;

		itemsCount[itemName]++;
	}
	for (auto const& itemCount : itemsCount)
		output << "\n" << itemCount.first << ": " << itemCount.second;
}

void QuakeAIAnalyzerView::AnalyzeAIGame(unsigned short tabIndex, 
	unsigned short gameFrame, unsigned short analysisFrame, unsigned short playerIndex,
	const std::string& decisionCluster, const std::string& evaluationCluster, 
	const std::string& decisionFilter, const std::string& evaluationFilter)
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	AIAnalysis::GameAnalysis& gameAnalysis = aiManager->GetGameAnalysis();
	if (gameAnalysis.decisions.size() < analysisFrame)
		return;

	GameViewType viewType = playerIndex - 1 ? GV_AI : GV_HUMAN;
	AIAnalysis::GameDecision gameDecision = (*gameAnalysis.decisions.begin());
	std::vector<AIAnalysis::GameDecision>::iterator itGameDecision = gameAnalysis.decisions.begin() + analysisFrame;
	for (; itGameDecision != gameAnalysis.decisions.begin(); itGameDecision--)
	{
		gameDecision = *itGameDecision;
		if (gameDecision.evaluation.target == viewType)
			break;
	}
	if (gameDecision.evaluation.target != viewType)
		gameDecision = AIAnalysis::GameDecision();

	mStats.gameTime = gameDecision.time;

	const AIAnalysis::GameEvaluation& gameEvaluation = aiManager->GetGameEvaluation();
	std::shared_ptr<BaseGameView> humanView = GameApplication::Get()->GetGameView(GV_HUMAN);
	ActorId playerId = humanView->GetActorId();
	std::shared_ptr<BaseGameView> aiView = GameApplication::Get()->GetGameView(GV_AI);
	ActorId otherPlayerId = aiView->GetActorId();

	std::string form =
		"form_version[3]size[28,14]position[0.5,0.5]"
		"container[0,0]box[0,0;6,12;#666666]"
		"dropdown[1,0.5;4,0.75;player;Blue player (" +
		std::to_string(playerId) + "),Red player (" +
		std::to_string(otherPlayerId) + ");" + std::to_string(playerIndex) + "]";

	if (tabIndex == 1)
	{
		form += "button_exit[2,10.5;2.5,0.75;btn_simulate;Watch]"
			"container_end[]";

		PathingNode* pathNode =
			aiManager->GetPathingGraph()->FindNode(gameEvaluation.playerInput.planNode);
		PathingArc* pathArc = gameEvaluation.playerInput.planPath.empty() ? nullptr :
			aiManager->GetPathingGraph()->FindArc(gameEvaluation.playerInput.planPath.back());

		//player input/output summary
		std::stringstream playerInput;
		playerInput.setf(std::ios::fixed);
		playerInput << "PlayerInput:\n"
			"\nid: " << gameEvaluation.playerInput.id <<
			"\ngame frame: " << gameEvaluation.playerInput.frame <<
			"\nweapon select: " << gameEvaluation.playerInput.weapon <<
			"\nweapon time: " << gameEvaluation.playerInput.weaponTime <<
			"\nweapon target: " << gameEvaluation.playerInput.target <<
			"\nhealth: " << gameEvaluation.playerInput.stats[STAT_HEALTH] <<
			"\narmor: " << gameEvaluation.playerInput.stats[STAT_ARMOR] <<
			"\nplan id: " << gameEvaluation.playerInput.planId <<
			"\nplan offset: " << gameEvaluation.playerInput.planOffset;

		if (pathNode)
		{
			playerInput <<
				"\nnode start: " << pathNode->GetId() <<
				"\ncluster start: " << pathNode->GetCluster();
		}
		else
		{
			playerInput <<
				"\nnode start: " <<
				"\ncluster start: ";
		}
		if (pathArc)
		{
			playerInput << 
				"\nnode end: " << pathArc->GetNode()->GetId() <<
				"\ncluster end: " << pathArc->GetNode()->GetCluster();
		}
		else if (pathNode)
		{
			playerInput << 
				"\nnode end: " << pathNode->GetId() <<
				"\ncluster end: " << pathNode->GetCluster();
		}
		else
		{
			playerInput <<
				"\nnode end: " <<
				"\ncluster end: ";
		}

		pathNode = aiManager->GetPathingGraph()->FindNode(gameEvaluation.playerGuessInput.planNode);
		pathArc = gameEvaluation.playerGuessInput.planPath.empty() ? nullptr :
			aiManager->GetPathingGraph()->FindArc(gameEvaluation.playerGuessInput.planPath.back());

		std::stringstream playerGuessInput;
		playerGuessInput.setf(std::ios::fixed);
		playerGuessInput << "PlayerGuessInput:\n"
			"\nid: " << gameEvaluation.playerGuessInput.id <<
			"\ngame frame: " << gameEvaluation.playerGuessInput.frame <<
			"\nweapon select: " << gameEvaluation.playerGuessInput.weapon <<
			"\nweapon time: " << gameEvaluation.playerGuessInput.weaponTime <<
			"\nweapon target: " << gameEvaluation.playerGuessInput.target <<
			"\nhealth: " << gameEvaluation.playerGuessInput.stats[STAT_HEALTH] <<
			"\narmor: " << gameEvaluation.playerGuessInput.stats[STAT_ARMOR] <<
			"\nplan id: " << gameEvaluation.playerGuessInput.planId <<
			"\nplan offset: " << gameEvaluation.playerGuessInput.planOffset;

		if (pathNode)
		{
			playerGuessInput <<
				"\nnode start: " << pathNode->GetId() <<
				"\ncluster start: " << pathNode->GetCluster();
		}
		else
		{
			playerGuessInput <<
				"\nnode start: " <<
				"\ncluster start: ";
		}
		if (pathArc)
		{
			playerGuessInput <<
				"\nnode end: " << pathArc->GetNode()->GetId() <<
				"\ncluster end: " << pathArc->GetNode()->GetCluster();
		}
		else if (pathNode)
		{
			playerGuessInput <<
				"\nnode end: " << pathNode->GetId() <<
				"\ncluster end: " << pathNode->GetCluster();
		}
		else
		{
			playerGuessInput <<
				"\nnode end: " <<
				"\ncluster end: ";
		}

		pathNode = aiManager->GetPathingGraph()->FindNode(gameEvaluation.playerOutput.planNode);
		pathArc = gameEvaluation.playerOutput.planPath.empty() ? nullptr :
			aiManager->GetPathingGraph()->FindArc(gameEvaluation.playerOutput.planPath.back());

		std::stringstream playerOutput;
		playerOutput.setf(std::ios::fixed);
		playerOutput << "PlayerOutput:\n"
			"\nid: " << gameEvaluation.playerOutput.id <<
			"\ngame frame: " << gameEvaluation.playerOutput.frame <<
			"\nweapon select: " << gameEvaluation.playerOutput.weapon <<
			"\nweapon target: " << gameEvaluation.playerOutput.target <<
			"\nweapon damage: " << gameEvaluation.playerOutput.damage <<
			"\nheuristic: " << gameEvaluation.playerOutput.heuristic <<
			"\nplan id: " << gameEvaluation.playerOutput.planId;

		if (pathNode)
		{
			playerOutput <<
				"\nnode start: " << pathNode->GetId() <<
				"\ncluster start: " << pathNode->GetCluster();
		}
		else
		{
			playerOutput <<
				"\nnode start: " <<
				"\ncluster start: ";
		}
		if (pathArc)
		{
			playerOutput <<
				"\nnode end: " << pathArc->GetNode()->GetId() <<
				"\ncluster end: " << pathArc->GetNode()->GetCluster();
		}
		else if (pathNode)
		{
			playerOutput <<
				"\nnode end: " << pathNode->GetId() <<
				"\ncluster end: " << pathNode->GetCluster();
		}
		else
		{
			playerOutput <<
				"\nnode end: " <<
				"\ncluster end: ";
		}

		PrintPlayerItems(gameEvaluation.playerOutput.items, playerOutput);

		pathNode = aiManager->GetPathingGraph()->FindNode(gameEvaluation.playerGuessOutput.planNode);
		pathArc = gameEvaluation.playerGuessOutput.planPath.empty() ? nullptr :
			aiManager->GetPathingGraph()->FindArc(gameEvaluation.playerGuessOutput.planPath.back());

		std::stringstream playerGuessOutput;
		playerGuessOutput.setf(std::ios::fixed);
		playerGuessOutput << "PlayerGuessOutput:\n"
			"\nid: " << gameEvaluation.playerGuessOutput.id <<
			"\ngame frame: " << gameEvaluation.playerGuessOutput.frame <<
			"\nweapon select: " << gameEvaluation.playerGuessOutput.weapon <<
			"\nweapon target: " << gameEvaluation.playerGuessOutput.target <<
			"\nweapon damage: " << gameEvaluation.playerGuessOutput.damage <<
			"\nheuristic: " << gameEvaluation.playerGuessOutput.heuristic <<
			"\nplan id: " << gameEvaluation.playerGuessOutput.planId;

		if (pathNode)
		{
			playerGuessOutput <<
				"\nnode start: " << pathNode->GetId() <<
				"\ncluster start: " << pathNode->GetCluster();
		}
		else
		{
			playerGuessOutput <<
				"\nnode start: " <<
				"\ncluster start: ";
		}
		if (pathArc)
		{
			playerGuessOutput <<
				"\nnode end: " << pathArc->GetNode()->GetId() <<
				"\ncluster end: " << pathArc->GetNode()->GetCluster();
		}
		else if (pathNode)
		{
			playerGuessOutput <<
				"\nnode end: " << pathNode->GetId() <<
				"\ncluster end: " << pathNode->GetCluster();
		}
		else
		{
			playerGuessOutput <<
				"\nnode end: " <<
				"\ncluster end: ";
		}

		PrintPlayerItems(gameEvaluation.playerGuessOutput.items, playerGuessOutput);

		form += "container[0,0]"
			"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
			"box[6.5,0;21.5,12;#666666]"
			"textarea[7,0.5;5.5,11;;" + playerInput.str() + ";]"
			"textarea[12.25,0.5;5.5,11;;" + playerGuessInput.str() + ";]"
			"textarea[17.5,0.5;5.5,11;;" + playerOutput.str() + ";]"
			"textarea[22.75,0.5;5.5,11;;" + playerGuessOutput.str() + ";]"
			"container_end[]";
	}
	else if (tabIndex == 2)
	{
		PlayerData playerGuessData, otherPlayerGuessData;
		aiManager->GetPlayerInput(gameDecision.evaluation.playerGuessInput, playerGuessData);
		aiManager->GetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, otherPlayerGuessData);

		// guessing minimax
		form += "field[1,1.5;3.25,0.75;te_search_evaluation;;]"
			"field_close_on_enter[te_search_evaluation;false]container[4.25,1.5]"
			"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search_evaluation;]"
			"tooltip[btn_mp_search_evaluation;Search]container_end[]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=2;text,align=inline,width=4.5;"
			"text,align=inline,width=1.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[1,2.25;4,3;evaluation_cluster;#4bdd42,Guessing(";
		form += std::to_string(gameEvaluation.playerGuessInput.id) + "),,,";

		std::vector<AIAnalysis::GameSimulation*> playerGuessings;
		if (!evaluationFilter.empty())
		{
			for (auto const& playerGuessing : gameEvaluation.playerGuessings)
			{
				std::string str = std::to_string(playerGuessing->clusters.back());
				if (evaluationFilter == str)
				{
					form += ",,,#ffffff," + 
						std::to_string(playerGuessing->clusters.front()) + "|" + 
						std::to_string(playerGuessing->clusters.back()) + "," +
						std::to_string(playerGuessing->action);

					playerGuessings.push_back(playerGuessing);
				}
			}
		}
		else
		{
			for (auto const& playerGuessing : gameEvaluation.playerGuessings)
			{
				form += ",,,#ffffff," + 
					std::to_string(playerGuessing->clusters.front()) + "|" +
					std::to_string(playerGuessing->clusters.back()) + "," +
					std::to_string(playerGuessing->action);
			}

			playerGuessings = gameEvaluation.playerGuessings;
		}

		unsigned short rowGuessCluster = 1;
		for (auto const& playerGuessing : playerGuessings)
		{
			std::string playerGuessingCluster = std::to_string(rowGuessCluster+1);
			if (playerGuessingCluster == evaluationCluster)
				break;

			rowGuessCluster++;
		}
		rowGuessCluster = playerGuessings.size() >= rowGuessCluster ? rowGuessCluster + 1 : 1;
		form += ";" + std::to_string(rowGuessCluster) + "]"
			"field[1,6.25;3.25,0.75;te_search_decision;;]"
			"field_close_on_enter[te_search_decision;false]container[4.25,6.25]"
			"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search_decision;]"
			"tooltip[btn_mp_search_decision;Search]container_end[]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=2;text,align=inline,width=4.5;"
			"text,align=inline,width=1.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[1,7;4,3;decision_cluster;#4bdd42,Guessing(";
		form += std::to_string(gameEvaluation.otherPlayerGuessInput.id) + "),,,";
			
		if (gameEvaluation.playerGuessings.size())
		{
			AIAnalysis::GameSimulation* playerGuessing = rowGuessCluster > 1 ?
				playerGuessings[rowGuessCluster - 2] : (*gameEvaluation.playerGuessings.begin());

			std::vector<AIAnalysis::Simulation*> playerGuessingSimulations;
			if (!decisionFilter.empty())
			{
				for (auto const& simulation : playerGuessing->simulations)
				{
					std::string str = std::to_string(simulation->otherPlayerSimulation.clusters.back());
					if (decisionFilter == str)
					{
						form += ",,,#ffffff," +
							std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
							std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
							std::to_string(simulation->otherPlayerSimulation.action);

						playerGuessingSimulations.push_back(simulation);
					}
				}
			}
			else
			{
				for (auto const& simulation : playerGuessing->simulations)
				{
					form += ",,,#ffffff," +
						std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
						std::to_string(simulation->otherPlayerSimulation.action);
				}

				playerGuessingSimulations = playerGuessing->simulations;
			}

			unsigned short otherRowGuessCluster = 1;
			ptrdiff_t otherRowGuessSimulation = 0;
			for (auto const& simulation : playerGuessingSimulations)
			{
				std::string otherPlayerSimulationCluster = std::to_string(otherRowGuessCluster+1);
				if (otherPlayerSimulationCluster == decisionCluster)
				{
					auto it = std::find(playerGuessing->simulations.begin(), playerGuessing->simulations.end(), simulation);
					otherRowGuessSimulation = std::distance(playerGuessing->simulations.begin(), it) + 2;
					aiManager->SetGameSimulation(simulation);
					break;
				}

				otherRowGuessCluster++;
			}
			otherRowGuessCluster = playerGuessingSimulations.size() >= otherRowGuessCluster ? otherRowGuessCluster + 1 : 1;
			if (rowGuessCluster > 1 && otherRowGuessCluster > 1)
			{
				form += ";" + std::to_string(otherRowGuessCluster) + "]"
					"button_exit[2,10.5;2.5,0.75;btn_simulate;Simulate]"
					"container_end[]";
			}
			else
			{
				form += ";" + std::to_string(otherRowGuessCluster) + "]"
					"container_end[]";
			}

			if (rowGuessCluster > 1)
			{
				form += "container[0,0]"
					"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
					"box[6.5,0;21.5,12;#666666]"
					"tablecolumns[color,span=1;text,align=inline;"
					"color,span=9;text,align=inline,width=6.5;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=6.5;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25]"
					"tableoptions[background=#00000000;border=false]"
					"table[7,0.5;20,11;graph_nodes;#4bdd42,Minimax(clusters action weapon damage heuristic),,,,,,,,,,";

				float otherPlayerHeuristic = FLT_MAX;
				ptrdiff_t simulationRow = 2;
				for (auto const& simulation : playerGuessing->simulations)
				{
					form += ",,,#ffffff," +
						std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->playerSimulation.clusters.back()) + "," +
						std::to_string(simulation->playerSimulation.action) + "," +
						std::to_string(simulation->playerSimulation.weapon) + "," +
						std::to_string(simulation->playerSimulation.damage) + "," +
						std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
						std::to_string(simulation->otherPlayerSimulation.action) + "," +
						std::to_string(simulation->otherPlayerSimulation.weapon) + "," +
						std::to_string(simulation->otherPlayerSimulation.damage) + "," +
						std::to_string(simulation->otherPlayerSimulation.heuristic);

					if (otherRowGuessCluster == 1)
					{
						if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
						{
							otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;
							otherRowGuessSimulation = simulationRow;
						}

						simulationRow++;
					}
				}

				if (otherRowGuessSimulation)
					form += ";" + std::to_string(otherRowGuessSimulation);
				form += "]container_end[]";
			}
			else
			{
				form += "container[0,0]"
					"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
					"box[6.5,0;21.5,12;#666666]"
					"container_end[]";
			}
		}
		else
		{
			form += "]container_end[]container[0,0]"
				"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
				"box[6.5,0;21.5,12;#666666]"
				"container_end[]";
		}
	}
	else if (tabIndex == 3)
	{
		PlayerData playerGuessData, otherPlayerGuessData;
		aiManager->GetPlayerInput(gameDecision.evaluation.playerGuessInput, playerGuessData);
		aiManager->GetPlayerInput(gameDecision.evaluation.otherPlayerGuessInput, otherPlayerGuessData);

		form += "field[1,1.5;3.25,0.75;te_search_evaluation;;]"
			"field_close_on_enter[te_search_evaluation;false]container[4.25,1.5]"
			"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search_evaluation;]"
			"tooltip[btn_mp_search_evaluation;Search]container_end[]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=2;text,align=inline,width=4.5;"
			"text,align=inline,width=1.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[1,2.25;4,3;evaluation_cluster;#4bdd42,Guessing(";
		form += std::to_string(gameEvaluation.playerGuessInput.id) + "),,,";

		unsigned short otherRowGuessCluster = 1;
		ptrdiff_t otherRowGuessSimulation = 0;
		if (gameEvaluation.playerGuessDecision)
		{
			std::vector<AIAnalysis::Simulation*> playerGuessDecisionSimulations;
			if (!evaluationFilter.empty())
			{
				for (auto const& simulation : gameEvaluation.playerGuessDecision->simulations)
				{
					std::string str = std::to_string(simulation->playerSimulation.clusters.back());
					if (evaluationFilter == str)
					{
						form += ",,,#ffffff," +
							std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
							std::to_string(simulation->playerSimulation.clusters.back()) + "," +
							std::to_string(simulation->playerSimulation.action);

						playerGuessDecisionSimulations.push_back(simulation);
					}
				}
			}
			else
			{
				for (auto const& simulation : gameEvaluation.playerGuessDecision->simulations)
				{
					form += ",,,#ffffff," +
						std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->playerSimulation.clusters.back()) + "," +
						std::to_string(simulation->playerSimulation.action);
				}

				playerGuessDecisionSimulations = gameEvaluation.playerGuessDecision->simulations;
			}

			for (auto const& simulation : playerGuessDecisionSimulations)
			{
				std::string playerSimulationCluster = std::to_string(otherRowGuessCluster+1);
				if (playerSimulationCluster == evaluationCluster)
				{
					auto it = std::find(gameEvaluation.playerGuessDecision->simulations.begin(), 
						gameEvaluation.playerGuessDecision->simulations.end(), simulation);
					otherRowGuessSimulation = std::distance(gameEvaluation.playerGuessDecision->simulations.begin(), it) + 2;
					aiManager->SetGameSimulation(simulation);
					break;
				}

				otherRowGuessCluster++;
			}

			otherRowGuessCluster = 
				playerGuessDecisionSimulations.size() >= otherRowGuessCluster ? otherRowGuessCluster + 1 : 1;
			if (otherRowGuessCluster > 1)
			{
				form += ";" + std::to_string(otherRowGuessCluster) + "]"
					"button_exit[2,10.5;2.5,0.75;btn_simulate;Simulate]"
					"container_end[]";
			}
			else
			{
				form += ";" + std::to_string(otherRowGuessCluster) + "]"
					"container_end[]";
			}
		}
		else form += "]container_end[]";

		form += "container[0,0]"
			"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
			"box[6.5,0;21.5,12;#666666]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=9;text,align=inline,width=6.5;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=6.5;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[7,0.5;20,11;graph_nodes;#4bdd42,Guess Decision(clusters action weapon damage heuristic),,,,,,,,,,";
		if (gameEvaluation.playerGuessDecision)
		{
			float playerHeuristic = -FLT_MAX;
			ptrdiff_t simulationRow = 2;
			for (auto const& simulation : gameEvaluation.playerGuessDecision->simulations)
			{
				form += ",,,#ffffff," +
					std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
					std::to_string(simulation->playerSimulation.clusters.back()) + "," +
					std::to_string(simulation->playerSimulation.action) + "," +
					std::to_string(simulation->playerSimulation.weapon) + "," +
					std::to_string(simulation->playerSimulation.damage) + "," +
					std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
					std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
					std::to_string(simulation->otherPlayerSimulation.action) + "," +
					std::to_string(simulation->otherPlayerSimulation.weapon) + "," +
					std::to_string(simulation->otherPlayerSimulation.damage) + "," +
					std::to_string(simulation->otherPlayerSimulation.heuristic);

				if (otherRowGuessCluster == 1)
				{
					if (simulation->playerSimulation.heuristic > playerHeuristic)
					{
						playerHeuristic = simulation->playerSimulation.heuristic;
						otherRowGuessSimulation = simulationRow;
					}

					simulationRow++;
				}
			}
			
			if (otherRowGuessSimulation)
				form += ";" + std::to_string(otherRowGuessSimulation);
		}
		form += "]container_end[]";
	}
	else if (tabIndex == 4)
	{
		PlayerData playerData, otherPlayerData;
		aiManager->GetPlayerInput(gameDecision.evaluation.playerInput, playerData);
		aiManager->GetPlayerInput(gameDecision.evaluation.otherPlayerInput, otherPlayerData);

		// decision minimax
		form += "field[1,1.5;3.25,0.75;te_search_evaluation;;]"
			"field_close_on_enter[te_search_evaluation;false]container[4.25,1.5]"
			"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search_evaluation;]"
			"tooltip[btn_mp_search_evaluation;Search]container_end[]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=2;text,align=inline,width=4.5;"
			"text,align=inline,width=1.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[1,2.25;4,3;evaluation_cluster;#4bdd42,Deciding(";
		form += std::to_string(gameEvaluation.playerInput.id) + "),,,";

			
		std::vector<AIAnalysis::GameSimulation*> playerDecisions;
		if (!evaluationFilter.empty())
		{
			for (auto const& playerDecision : gameEvaluation.playerDecisions)
			{
				std::string str = std::to_string(playerDecision->clusters.back());
				if (evaluationFilter == str)
				{
					form += ",,,#ffffff," +
						std::to_string(playerDecision->clusters.front()) + "|" +
						std::to_string(playerDecision->clusters.back()) + "," +
						std::to_string(playerDecision->action);

					playerDecisions.push_back(playerDecision);
				}
			}
		}
		else
		{
			for (auto const& playerDecision : gameEvaluation.playerDecisions)
			{
				form += ",,,#ffffff," +
					std::to_string(playerDecision->clusters.front()) + "|" +
					std::to_string(playerDecision->clusters.back()) + "," +
					std::to_string(playerDecision->action);
			}
			playerDecisions = gameEvaluation.playerDecisions;
		}

		unsigned short rowDecisionCluster = 1;
		for (auto const& playerDecision : playerDecisions)
		{
			std::string playerDecisionCluster = std::to_string(rowDecisionCluster+1);
			if (playerDecisionCluster == evaluationCluster)
				break;

			rowDecisionCluster++;
		}
		rowDecisionCluster = playerDecisions.size() >= rowDecisionCluster ? rowDecisionCluster + 1 : 1;
		form += ";" + std::to_string(rowDecisionCluster) + "]"
			"field[1,6.25;3.25,0.75;te_search_decision;;]"
			"field_close_on_enter[te_search_decision;false]container[4.25,6.25]"
			"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search_decision;]"
			"tooltip[btn_mp_search_decision;Search]container_end[]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=2;text,align=inline,width=4.5;"
			"text,align=inline,width=1.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[1,7;4,3;decision_cluster;#4bdd42,Deciding(";
		form += std::to_string(gameEvaluation.otherPlayerInput.id) + "),,,";

		if (gameEvaluation.playerDecisions.size())
		{
			AIAnalysis::GameSimulation* playerDecision = rowDecisionCluster > 1 ?
				playerDecisions[rowDecisionCluster - 2] : (*gameEvaluation.playerDecisions.begin());

			std::vector<AIAnalysis::Simulation*> playerDecisionSimulations;
			if (!decisionFilter.empty())
			{
				for (auto const& simulation : playerDecision->simulations)
				{
					std::string str = std::to_string(simulation->otherPlayerSimulation.clusters.back());
					if (decisionFilter == str)
					{
						form += ",,,#ffffff," +
							std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
							std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
							std::to_string(simulation->otherPlayerSimulation.action);

						playerDecisionSimulations.push_back(simulation);
					}
				}
			}
			else
			{
				for (auto const& simulation : playerDecision->simulations)
				{
					form += ",,,#ffffff," +
						std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
						std::to_string(simulation->otherPlayerSimulation.action);
				}

				playerDecisionSimulations = playerDecision->simulations;
			}

			unsigned short otherRowDecisionCluster = 1;
			ptrdiff_t otherRowDecisionSimulation = 0;
			for (auto const& simulation : playerDecisionSimulations)
			{
				std::string otherPlayerSimulationCluster = std::to_string(otherRowDecisionCluster+1);
				if (otherPlayerSimulationCluster == decisionCluster)
				{
					auto it = std::find(playerDecision->simulations.begin(), playerDecision->simulations.end(), simulation);
					otherRowDecisionSimulation = std::distance(playerDecision->simulations.begin(), it) + 2;
					aiManager->SetGameSimulation(simulation);
					break;
				}

				otherRowDecisionCluster++;
			}
			otherRowDecisionCluster = playerDecisionSimulations.size() >= otherRowDecisionCluster ? otherRowDecisionCluster + 1 : 1;
			if (rowDecisionCluster > 1 && otherRowDecisionCluster > 1)
			{
				form += ";" + std::to_string(otherRowDecisionCluster) + "]"
					"button_exit[2,10.5;2.5,0.75;btn_simulate;Simulate]"
					"container_end[]";
			}
			else
			{
				form += ";" + std::to_string(otherRowDecisionCluster) + "]"
					"container_end[]";
			}

			if (rowDecisionCluster > 1)
			{
				form += "container[0,0]"
					"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
					"box[6.5,0;21.5,12;#666666]"
					"tablecolumns[color,span=1;text,align=inline;"
					"color,span=9;text,align=inline,width=6.5;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=6.5;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25;"
					"text,align=inline,width=3.25]"
					"tableoptions[background=#00000000;border=false]"
					"table[7,0.5;20,11;graph_nodes;#4bdd42,Minimax(clusters action weapon damage heuristic),,,,,,,,,,";

				float otherPlayerHeuristic = FLT_MAX;
				int simulationRow = 2;
				for (auto const& simulation : playerDecision->simulations)
				{
					form += ",,,#ffffff," +
						std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->playerSimulation.clusters.back()) + "," +
						std::to_string(simulation->playerSimulation.action) + "," +
						std::to_string(simulation->playerSimulation.weapon) + "," +
						std::to_string(simulation->playerSimulation.damage) + "," +
						std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
						std::to_string(simulation->otherPlayerSimulation.action) + "," +
						std::to_string(simulation->otherPlayerSimulation.weapon) + "," +
						std::to_string(simulation->otherPlayerSimulation.damage) + "," +
						std::to_string(simulation->otherPlayerSimulation.heuristic);

					if (otherRowDecisionCluster == 1)
					{
						if (simulation->otherPlayerSimulation.heuristic < otherPlayerHeuristic)
						{
							otherPlayerHeuristic = simulation->otherPlayerSimulation.heuristic;
							otherRowDecisionSimulation = simulationRow;
						}

						simulationRow++;
					}
				}

				if (otherRowDecisionSimulation)
					form += ";" + std::to_string(otherRowDecisionSimulation);
				form += "]container_end[]";
			}
			else
			{
				form += "container[0,0]"
					"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
					"box[6.5,0;21.5,12;#666666]"
					"container_end[]";
			}
		}
		else
		{
			form += "]container_end[]container[0,0]"
				"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
				"box[6.5,0;21.5,12;#666666]"
				"container_end[]";
		}
	}
	else if (tabIndex == 5)
	{
		PlayerData playerData, otherPlayerData;
		aiManager->GetPlayerInput(gameDecision.evaluation.playerInput, playerData);
		aiManager->GetPlayerInput(gameDecision.evaluation.otherPlayerInput, otherPlayerData);

		form += "field[1,1.5;3.25,0.75;te_search_decision;;]"
			"field_close_on_enter[te_search_decision;false]container[4.25,1.5]"
			"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search_decision;]"
			"tooltip[btn_mp_search_decision;Search]container_end[]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=2;text,align=inline,width=4.5;"
			"text,align=inline,width=1.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[1,2.25;4,3;decision_cluster;#4bdd42,Deciding(";
		form += std::to_string(gameEvaluation.playerInput.id) + "),,,";

		unsigned short otherRowDecisionCluster = 1;
		ptrdiff_t otherRowDecisionSimulation = 0;
		if (gameEvaluation.playerDecision)
		{
			std::vector<AIAnalysis::Simulation*> playerDecisionSimulations;
			if (!decisionFilter.empty())
			{
				for (auto const& simulation : gameEvaluation.playerDecision->simulations)
				{
					std::string str = std::to_string(simulation->playerSimulation.clusters.back());
					if (decisionFilter == str)
					{
						form += ",,,#ffffff," +
							std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
							std::to_string(simulation->playerSimulation.clusters.back()) + "," +
							std::to_string(simulation->playerSimulation.action);

						playerDecisionSimulations.push_back(simulation);
					}
				}
			}
			else
			{
				for (auto const& simulation : gameEvaluation.playerDecision->simulations)
				{
					form += ",,,#ffffff," +
						std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
						std::to_string(simulation->playerSimulation.clusters.back()) + "," +
						std::to_string(simulation->playerSimulation.action);
				}

				playerDecisionSimulations = gameEvaluation.playerDecision->simulations;
			}

			for (auto const& playerDecisionSimulation : playerDecisionSimulations)
			{
				std::string playerDecisionCluster = std::to_string(otherRowDecisionCluster+1);
				if (playerDecisionCluster == decisionCluster)
				{
					auto it = std::find(gameEvaluation.playerDecision->simulations.begin(), 
						gameEvaluation.playerDecision->simulations.end(), playerDecisionSimulation);
					otherRowDecisionSimulation = std::distance(gameEvaluation.playerDecision->simulations.begin(), it) + 2;
					aiManager->SetGameSimulation(playerDecisionSimulation);
					break;
				}

				otherRowDecisionCluster++;
			}
			otherRowDecisionCluster = playerDecisionSimulations.size() >= otherRowDecisionCluster ? otherRowDecisionCluster + 1 : 1;
			if (otherRowDecisionCluster > 1)
			{
				form += ";" + std::to_string(otherRowDecisionCluster) + "]"
					"button_exit[2,10.5;2.5,0.75;btn_simulate;Simulate]"
					"container_end[]";
			}
			else
			{
				form += ";" + std::to_string(otherRowDecisionCluster) + "]"
					"container_end[]";
			}
		}
		else form += "]container_end[]";

		form += "container[0,0]"
			"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
			"box[6.5,0;21.5,12;#666666]"
			"tablecolumns[color,span=1;text,align=inline;"
			"color,span=9;text,align=inline,width=6.5;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=6.5;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25;"
			"text,align=inline,width=3.25]"
			"tableoptions[background=#00000000;border=false]"
			"table[7,0.5;20,11;graph_nodes;#4bdd42,Decision(clusters action weapon damage heuristic),,,,,,,,,,";
		if (gameEvaluation.playerDecision)
		{
			float playerHeuristic = -FLT_MAX;
			ptrdiff_t simulationRow = 2;
			for (auto const& simulation : gameEvaluation.playerDecision->simulations)
			{
				form += ",,,#ffffff," +
					std::to_string(simulation->playerSimulation.clusters.front()) + "|" +
					std::to_string(simulation->playerSimulation.clusters.back()) + "," +
					std::to_string(simulation->playerSimulation.action) + "," +
					std::to_string(simulation->playerSimulation.weapon) + "," +
					std::to_string(simulation->playerSimulation.damage) + "," +
					std::to_string(simulation->otherPlayerSimulation.clusters.front()) + "|" +
					std::to_string(simulation->otherPlayerSimulation.clusters.back()) + "," +
					std::to_string(simulation->otherPlayerSimulation.action) + "," +
					std::to_string(simulation->otherPlayerSimulation.weapon) + "," +
					std::to_string(simulation->otherPlayerSimulation.damage) + "," +
					std::to_string(simulation->otherPlayerSimulation.heuristic);

				if (otherRowDecisionCluster == 1)
				{
					if (simulation->playerSimulation.heuristic > playerHeuristic)
					{
						playerHeuristic = simulation->playerSimulation.heuristic;
						otherRowDecisionSimulation = simulationRow;
					}

					simulationRow++;
				}
			}
			
			if (otherRowDecisionSimulation)
				form += ";" + std::to_string(otherRowDecisionSimulation);
		}
		form += "]container_end[]";
	}
	else
	{
		form += "container[0,0]"
			"tabheader[6.5,0;maintab;Summary,Minimax,Guess,Minimax,Decision;" + std::to_string(tabIndex) + ";true;false]"
			"box[6.5,0;21.5,12;#666666]"
			"container_end[]";
	}
	
	form +=
		"field[7.5,12.75;2,0.75;te_search;;" + std::to_string(analysisFrame) +
		"]field_close_on_enter[te_search;false]container[2.5,0.75]"
		"image_button[7,12;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"tooltip[btn_mp_search;Search]container_end[]"
		"scrollbaroptions[max=" + std::to_string(gameAnalysis.decisions.size() - 1) + ";smallstep=1]"
		"scrollbar[20.5,13.5;10,0.75;horizontal;scrbar;" + std::to_string(analysisFrame) + "]";

	/* Create menu */
	/* Note: FormSource and AIAnalysisFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<AIAnalysisFormHandler> textDst = std::make_shared<AIAnalysisFormHandler>("ANALYZE_GAME");
	textDst->mGameFrame = gameFrame;
	textDst->mAnalysisFrame = analysisFrame;
	textDst->mPlayerIndex = playerIndex;
	textDst->mDecisionCluster[tabIndex - 1] = decisionCluster;
	textDst->mEvaluationCluster[tabIndex - 1] = evaluationCluster;
	textDst->mDecisionFilter[tabIndex - 1] = decisionFilter;
	textDst->mEvaluationFilter[tabIndex - 1] = evaluationFilter;
	textDst->mTabIndex = tabIndex;

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "ANALYZE_GAME")
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("ANALYZE_GAME");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIAnalyzerView::ShowAIGameAnalysis(unsigned short tabIndex, 
	unsigned short gameFrame, unsigned short analysisFrame, unsigned short playerIndex,
	const std::string& decisionCluster, const std::string& evaluationCluster, 
	const std::string& decisionFilter, const std::string& evaluationFilter)
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());

	size_t simulationSteps = 0;
	if (mGameAISimulation)
	{
		const AIAnalysis::GameAnalysis& gameAnalysis = aiManager->GetGameAnalysis();
		if (gameAnalysis.decisions.size() < analysisFrame)
			return;

		const AIAnalysis::GameDecision gameDecision = gameAnalysis.decisions[analysisFrame];
		mStats.gameTime = gameDecision.time;

		AIAnalysis::Simulation* gameSimulation = aiManager->GetGameSimulation();
		if (!gameSimulation)
			return;

		float playerPathWeight = 0.f;
		PathingNode* pathingNode = aiManager->GetPathingGraph()->FindNode(mPlayerInput.planNode);
		for (int pathArc : gameSimulation->playerSimulation.planPath)
		{
			PathingArc* pathingArc = pathingNode->FindArc(pathArc);

			playerPathWeight += pathingArc->GetWeight();
			pathingNode = pathingArc->GetNode();
		}
		playerPathWeight -= mPlayerInput.planOffset;

		float otherPlayerPathWeight = 0.f;
		pathingNode = aiManager->GetPathingGraph()->FindNode(mOtherPlayerInput.planNode);
		for (int pathArc : gameSimulation->otherPlayerSimulation.planPath)
		{
			PathingArc* pathingArc = pathingNode->FindArc(pathArc);

			otherPlayerPathWeight += pathingArc->GetWeight();
			pathingNode = pathingArc->GetNode();
		}
		otherPlayerPathWeight -= mOtherPlayerInput.planOffset;

		simulationSteps = playerPathWeight > otherPlayerPathWeight ?
			(size_t)ceil(playerPathWeight * 10) : (size_t)ceil(otherPlayerPathWeight * 10);
	}
	else simulationSteps = aiManager->GetGame().states.size() - 1;
          
	std::string form;
	if (mGameAISimulation)
		form = "form_version[3]size[18,2]position[0.5,0.9]field[1,0.75;1.75,0.75;te_search;;0";
	else
		form = "form_version[3]size[18,2]position[0.5,0.9]field[1,0.75;1.75,0.75;te_search;;" + std::to_string(gameFrame);

	form +=
		"]field_close_on_enter[te_search;false]container[2.75,0.75]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"tooltip[btn_mp_search;Search]container_end[]"
		"scrollbaroptions[max=" + std::to_string(simulationSteps) + ";smallstep=1]";

	if (mGameAISimulation)
		form += "scrollbar[14,1.5;10,0.75;horizontal;scrbar;0";
	else
		form += "scrollbar[14,1.5;10,0.75;horizontal;scrbar;" + std::to_string(gameFrame);

	form +=
		"]button_exit[15,0.75;1.5,0.8;btn_back;Back]";

	/* Create menu */
	/* Note: FormSource and AIGameSimulationFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<AIGameSimulationFormHandler> textDst = 
		std::make_shared<AIGameSimulationFormHandler>("SHOW_SIMULATION");
	textDst->mGameFrame = gameFrame;
	textDst->mAnalysisFrame = analysisFrame;
	textDst->mPlayerIndex = playerIndex;
	textDst->mDecisionCluster = decisionCluster;
	textDst->mEvaluationCluster = evaluationCluster;
	textDst->mDecisionFilter = decisionFilter;
	textDst->mEvaluationFilter = evaluationFilter;
	textDst->mTabIndex = tabIndex;

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "SHOW_SIMULATION")
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("SHOW_SIMULATION");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIAnalyzerView::ShowAIGame(const AIGame::Game& game)
{
	mGameAISimulation = false;

	std::string form =
		"form_version[3]size[16,2]position[0.5,0.9]"
		"field[1,0.75;1.5,0.75;te_search;;0]field_close_on_enter[te_search;false]container[2.5,0.75]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"tooltip[btn_mp_search;Search]container_end[]"
		"scrollbaroptions[max=" + std::to_string(game.states.size() - 1) + ";smallstep=1]"
		"scrollbar[14,1.5;10,0.75;horizontal;scrbar;0]";

	/* Create menu */
	/* Note: FormSource and AIGameFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<AIGameFormHandler> textDst = std::make_shared<AIGameFormHandler>("SHOW_GAME");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "SHOW_GAME")
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("SHOW_GAME");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIAnalyzerView::ShowPauseMenu()
{
	static const std::string controlTextTemplate = "Controls:\n"
		"- %s: move forwards\n"
		"- %s: move backwards\n"
		"- %s: move left\n"
		"- %s: move right\n"
		"- %s: move up\n"
		"- %s: move down\n"
		"- %s: select node\n"
		"- %s: physics wireframe\n"
		"- %s: graphics wireframe\n"
		"- %s: follow player\n"
		"- %s: control camera\n"
		"- %s: chat\n";

	char controlTextBuf[600];

	snprintf(controlTextBuf, sizeof(controlTextBuf), controlTextTemplate.c_str(),
		GetKeySetting("keymap_forward").Name(),
		GetKeySetting("keymap_backward").Name(),
		GetKeySetting("keymap_left").Name(),
		GetKeySetting("keymap_right").Name(),
		GetKeySetting("keymap_jump").Name(),
		GetKeySetting("keymap_camera_mode").Name(),
		GetKeySetting("keymap_dig").Name(),
		GetKeySetting("keymap_slot6").Name(),
		GetKeySetting("keymap_slot7").Name(),
		GetKeySetting("keymap_slot8").Name(),
		GetKeySetting("keymap_slot9").Name(),
		GetKeySetting("keymap_chat").Name()
	);

	std::string controlText = controlTextBuf;
	StringFormEscape(controlText);

	float yPos = 0.7f;
	std::ostringstream os;

	os << "form_version[1]" << SIZE_TAG
		<< "button_exit[4," << (yPos++) << ";3,0.5;btn_simulate_game;"
		<< "Simulate AI Game" << "]" << "field[4.95,0;5,1.5;;" << "Main Menu" << ";]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_analyze_game;"
		<< "Analyze AI Game" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_show_game;"
		<< "Show AI Game" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_save_game;"
		<< "Save AI Game" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_exit_menu;"
		<< "Exit" << "]"

		<< "textarea[7.5,0.25;3.9,6.25;;" << controlText << ";]"
		<< "textarea[0.4,0.25;3.9,6.25;;" << "Quake \n"
		<< "\n" << "Analyzer info:" << "\n";
	os << ";]";

	/* Create menu */
	/* Note: FormSource and AIAnalyzerFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(os.str());
	std::shared_ptr<AIAnalyzerFormHandler> textDst = std::make_shared<AIAnalyzerFormHandler>("PAUSE_MENU");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("PAUSE_MENU");
	if (formUI)
	{
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}

	formUI->SetFocus("btn_simulate_game");
}

void  QuakeAIAnalyzerView::UpdateControllers(unsigned int timeMs, unsigned long deltaMs)
{
	if (mCameraController)
		mCameraController->OnUpdate(timeMs, deltaMs);

	if (mPlayerController)
		mPlayerController->OnUpdate(timeMs, deltaMs);
}

void QuakeAIAnalyzerView::UpdateSound(float dTime)
{
	// Update sound listener
	mSoundMgr->UpdateListener(
		mCamera->GetRelativeTransform().GetTranslation(),
		Vector3<float>::Zero(), HProject(mCamera->Get()->GetDVector()),
		HProject(mCamera->Get()->GetUVector()));

	bool muteSound = Settings::Get()->GetBool("mute_sound");
	if (!muteSound)
	{
		// Check if volume is in the proper range, else fix it.
		float oldVolume = Settings::Get()->GetFloat("sound_volume");
		float newVolume = std::clamp(oldVolume, 0.0f, 1.0f);
		mSoundMgr->SetListenerGain(newVolume);

		if (oldVolume != newVolume)
			Settings::Get()->SetFloat("sound_volume", newVolume);
	}
	else
	{
		mSoundMgr->SetListenerGain(0.0f);
	}
	/*
	VisualPlayer* player = mEnvironment->GetPlayer();

	// Tell the sound maker whether to make footstep sounds
	mSoundMaker->mMakesFootstepSound = player->mMakesFootstepSound;

	//	Update sound maker
	if (player->mMakesFootstepSound)
		mSoundMaker->Step(dTime);

	std::shared_ptr<VisualMap> map = mEnvironment->GetVisualMap();
	MapNode node = map->GetNode(player->GetFootstepNodePosition());
	mSoundMaker->mPlayerStepSound = mEnvironment->GetNodeManager()->Get(node).soundFootstep;
	*/
}


void QuakeAIAnalyzerView::UpdateGameAISimulation(unsigned short frame)
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());

	AIAnalysis::Simulation* gameSimulation = aiManager->GetGameSimulation();
	for (auto const& item : gameSimulation->playerSimulation.items)
	{
		std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
		pItemNode->SetVisible(true);
	}
	for (auto const& item : gameSimulation->otherPlayerSimulation.items)
	{
		std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
		pItemNode->SetVisible(true);
	}

	float simulationWeight = mPlayerInput.planOffset + frame / 10.f;
	if (simulationWeight <= mPlayerInput.planOffset)
		return;

	float pathingWeight = 0.f;
	PathingNode* pathingNode = aiManager->GetPathingGraph()->FindNode(mPlayerInput.planNode);
	if (pathingNode->GetActorId() != INVALID_ACTOR_ID)
	{
		for (auto const& item : gameSimulation->playerSimulation.items)
		{
			if (pathingNode->GetActorId() == item.first)
			{
				std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
				pItemNode->SetVisible(false);
			}
		}
	}

	if (pathingWeight < simulationWeight)
	{
		for (int path : gameSimulation->playerSimulation.planPath)
		{
			PathingArc* pathingArc = pathingNode->FindArc(path);
			if (pathingWeight + pathingArc->GetWeight() > simulationWeight)
				break;

			if (gameSimulation->playerSimulation.items.find(pathingArc->GetNode()->GetActorId()) != 
				gameSimulation->playerSimulation.items.end())
			{
				std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(pathingArc->GetNode()->GetActorId());
				pItemNode->SetVisible(false);
			}

			pathingWeight += pathingArc->GetWeight();
			pathingNode = pathingArc->GetNode();
		}
	}

	std::shared_ptr<Node> pPlayerNode = mScene->GetSceneNode(mPlayerInput.id);
	if (pPlayerNode)
	{
		pathingWeight = 0.f;
		pathingNode = aiManager->GetPathingGraph()->FindNode(mPlayerInput.planNode);
		for (int path : gameSimulation->playerSimulation.planPath)
		{
			PathingArc* pathingArc = pathingNode->FindArc(path);
			if (pathingWeight + pathingArc->GetWeight() >= simulationWeight)
			{
				const std::vector<float>& frameWeights = pathingArc->GetTransition()->GetWeights();
				const std::vector<Vector3<float>>& framePositions = pathingArc->GetTransition()->GetPositions();

				unsigned short frameIdx = 0;
				for (; frameIdx < frameWeights.size(); frameIdx++)
				{
					pathingWeight += frameWeights[frameIdx];
					if (pathingWeight >= simulationWeight)
						break;
				}

				Vector3<float> framePosition = frameIdx == frameWeights.size() ?
					pathingArc->GetNode()->GetPosition() : framePositions[frameIdx];
				Vector4<float> direction = HLift(framePosition - pPlayerNode->GetAbsoluteTransform().GetTranslation(), 0.f);
				Normalize(direction);

				Matrix4x4<float> yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), atan2(direction[1], direction[0])));
				pPlayerNode->GetRelativeTransform().SetRotation(yawRotation);
				pPlayerNode->UpdateAbsoluteTransform();

				std::shared_ptr<PlayerActor> pPlayerActor(
					std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mPlayerInput.id).lock()));
				std::shared_ptr<TransformComponent> pTransformComponent(
					pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
				if (pTransformComponent)
				{
					// update node rotation matrix
					Matrix4x4<float> rollRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
					pTransformComponent->SetRotation(yawRotation * rollRotation);
				}
				break;
			}

			pathingWeight += pathingArc->GetWeight();
			pathingNode = pathingArc->GetNode();
		}

		if (pathingWeight < simulationWeight)
		{
			Vector4<float> direction = HLift(pathingNode->GetPosition() - pPlayerNode->GetAbsoluteTransform().GetTranslation(), 0.f);
			Normalize(direction);

			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), atan2(direction[1], direction[0])));
			pPlayerNode->GetRelativeTransform().SetRotation(yawRotation);
			pPlayerNode->UpdateAbsoluteTransform();

			std::shared_ptr<PlayerActor> pPlayerActor(
				std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mPlayerInput.id).lock()));
			std::shared_ptr<TransformComponent> pTransformComponent(
				pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				// update node rotation matrix
				Matrix4x4<float> rollRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
				pTransformComponent->SetRotation(yawRotation * rollRotation);
			}
		}
	}

	simulationWeight = mOtherPlayerInput.planOffset + frame / 10.f;

	pathingWeight = 0.f;
	pathingNode = aiManager->GetPathingGraph()->FindNode(mOtherPlayerInput.planNode);
	if (gameSimulation->otherPlayerSimulation.items.find(pathingNode->GetActorId()) != 
		gameSimulation->otherPlayerSimulation.items.end())
	{
		std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(pathingNode->GetActorId());
		pItemNode->SetVisible(false);
	}

	if (pathingWeight < simulationWeight)
	{
		for (int playerPath : gameSimulation->otherPlayerSimulation.planPath)
		{
			PathingArc* pathingArc = pathingNode->FindArc(playerPath);
			if (pathingWeight + pathingArc->GetWeight() > simulationWeight)
				break;

			if (gameSimulation->otherPlayerSimulation.items.find(pathingArc->GetNode()->GetActorId()) !=
				gameSimulation->otherPlayerSimulation.items.end())
			{
				std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(pathingArc->GetNode()->GetActorId());
				pItemNode->SetVisible(false);
			}

			pathingWeight += pathingArc->GetWeight();
			pathingNode = pathingArc->GetNode();
		}
	}

	pPlayerNode = mScene->GetSceneNode(mOtherPlayerInput.id);
	if (pPlayerNode)
	{
		pathingWeight = 0.f;
		pathingNode = aiManager->GetPathingGraph()->FindNode(mOtherPlayerInput.planNode);
		for (int path : gameSimulation->otherPlayerSimulation.planPath)
		{
			PathingArc* pathingArc = pathingNode->FindArc(path);
			if (pathingWeight + pathingArc->GetWeight() >= simulationWeight)
			{
				const std::vector<float>& frameWeights = pathingArc->GetTransition()->GetWeights();
				const std::vector<Vector3<float>>& framePositions = pathingArc->GetTransition()->GetPositions();

				unsigned short frameIdx = 0;
				for (; frameIdx < frameWeights.size(); frameIdx++)
				{
					pathingWeight += frameWeights[frameIdx];
					if (pathingWeight >= simulationWeight)
						break;
				}

				Vector3<float> framePosition = frameIdx == frameWeights.size() ?
					pathingArc->GetNode()->GetPosition() : framePositions[frameIdx];
				Vector4<float> direction = HLift(framePosition - pPlayerNode->GetAbsoluteTransform().GetTranslation(), 0.f);
				Normalize(direction);

				Matrix4x4<float> yawRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), atan2(direction[1], direction[0])));
				pPlayerNode->GetRelativeTransform().SetRotation(yawRotation);
				pPlayerNode->UpdateAbsoluteTransform();

				std::shared_ptr<PlayerActor> pPlayerActor(
					std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mOtherPlayerInput.id).lock()));
				std::shared_ptr<TransformComponent> pTransformComponent(
					pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
				if (pTransformComponent)
				{
					// update node rotation matrix
					Matrix4x4<float> rollRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
					pTransformComponent->SetRotation(yawRotation * rollRotation);
				}
				break;
			}

			pathingWeight += pathingArc->GetWeight();
			pathingNode = pathingArc->GetNode();
		}

		if (pathingWeight < simulationWeight)
		{
			Vector4<float> direction = HLift(pathingNode->GetPosition() - pPlayerNode->GetAbsoluteTransform().GetTranslation(), 0.f);
			Normalize(direction);

			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), atan2(direction[1], direction[0])));
			pPlayerNode->GetRelativeTransform().SetRotation(yawRotation);
			pPlayerNode->UpdateAbsoluteTransform();

			std::shared_ptr<PlayerActor> pPlayerActor(
				std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(mOtherPlayerInput.id).lock()));
			std::shared_ptr<TransformComponent> pTransformComponent(
				pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				// update node rotation matrix
				Matrix4x4<float> rollRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
				pTransformComponent->SetRotation(yawRotation * rollRotation);
			}
		}
	}
}

void QuakeAIAnalyzerView::UpdateGameAIState()
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	std::map<ActorId, ActorId>& gameActors = aiManager->GetGameActors();

	for (AIGame::Player player : mGameAIState.players)
	{
		std::shared_ptr<Node> pNode = mScene->GetSceneNode(player.id);
		if (pNode)
		{
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), player.yaw));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), player.pitch));

			pNode->GetRelativeTransform().SetRotation(yawRotation * pitchRotation);

			std::shared_ptr<PlayerActor> pPlayerActor(
				std::dynamic_pointer_cast<PlayerActor>(GameLogic::Get()->GetActor(player.id).lock()));
			std::shared_ptr<TransformComponent> pTransformComponent(
				pPlayerActor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
			if (pTransformComponent)
			{
				// update node rotation matrix
				Matrix4x4<float> rollRotation = Rotation<4, float>(
					AxisAngle<4, float>(Vector4<float>::Unit(AXIS_X), 90.f * (float)GE_C_DEG_TO_RAD));
				pTransformComponent->SetRotation(yawRotation * pitchRotation * rollRotation);
			}
		}
	}

	for (AIGame::Projectile projectile : mGameAIState.projectiles)
	{
		std::shared_ptr<Node> pNode = mScene->GetSceneNode(gameActors[projectile.id]);
		if (pNode)
		{
			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Y), projectile.yaw));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(Vector4<float>::Unit(AXIS_Z), projectile.pitch));

			pNode->GetRelativeTransform().SetRotation(yawRotation * pitchRotation);
		}
	}
}


void QuakeAIAnalyzerView::UpdateGameAIAnalysis(unsigned short tabIndex, unsigned short analysisFrame)
{
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	const AIAnalysis::GameAnalysis& gameAnalysis = aiManager->GetGameAnalysis();
	if (gameAnalysis.decisions.size() < analysisFrame)
		return;

	const AIAnalysis::GameEvaluation& gameEvaluation = aiManager->GetGameEvaluation();

	//Summary, Minimax, Guess, Minimax, Decision
	bool foundSimulation = false;
	if (tabIndex == 1)
	{
		mPlayerInput = gameEvaluation.playerInput;
		mOtherPlayerInput = gameEvaluation.otherPlayerInput;
	}
	else if (tabIndex == 2)
	{
		mPlayerInput = gameEvaluation.playerGuessInput;
		mOtherPlayerInput = gameEvaluation.otherPlayerGuessInput;

		for (auto const& item : gameEvaluation.playerGuessItems)
		{
			std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
			pItemNode->SetVisible(item.second <= 0);
		}
	}
	else if (tabIndex == 3)
	{
		mPlayerInput = gameEvaluation.playerGuessInput;
		mOtherPlayerInput = gameEvaluation.otherPlayerGuessInput;

		for (auto const& item : gameEvaluation.playerGuessItems)
		{
			std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
			pItemNode->SetVisible(item.second <= 0);
		}
	}
	else if (tabIndex == 4)
	{
		mPlayerInput = gameEvaluation.playerInput;
		mOtherPlayerInput = gameEvaluation.otherPlayerInput;

		for (auto const& item : gameEvaluation.playerDecisionItems)
		{
			std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
			pItemNode->SetVisible(item.second <= 0);
		}
	}
	else if (tabIndex == 5)
	{
		mPlayerInput = gameEvaluation.playerInput;
		mOtherPlayerInput = gameEvaluation.otherPlayerInput;

		for (auto const& item : gameEvaluation.playerDecisionItems)
		{
			std::shared_ptr<Node> pItemNode = mScene->GetSceneNode(item.first);
			pItemNode->SetVisible(item.second <= 0);
		}
	}
}

void QuakeAIAnalyzerView::ProcessPlayerInteraction(float dTime, bool showHud, bool showDebug)
{
	ClearWasKeyPressed();
	ClearWasKeyReleased();
}

bool QuakeAIAnalyzerView::InitSound()
{
	if (Settings::Get()->GetBool("enable_sound"))
	{
		//create soundmanager
		LogInformation("Attempting to use OpenAL audio");
		mSoundMgr = std::make_shared<OpenALSoundManager>(
			static_cast<OpenALSoundSystem*>(SoundSystem::Get()), &mSoundFetcher);
		if (!mSoundMgr)
		{
			LogError("Failed to Initialize OpenAL audio");
			return false;
		}
	}
	else LogInformation("Sound disabled.");

	mSoundMaker = std::make_shared<AnalyzerSoundMaker>(mSoundMgr.get());
	if (!mSoundMaker)
	{
		LogError("Failed to Initialize OpenAL audio");
		return false;
	}

	//mSoundMaker->RegisterReceiver(mEventMgr);

	return true;
}