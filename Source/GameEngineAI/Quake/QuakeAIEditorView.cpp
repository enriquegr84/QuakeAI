//========================================================================
// QuakeAIEditorView.cpp : Game View Class
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
#include "QuakeAIEditorView.h"

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
#include "Games/Actors/LocationTarget.h"
#include "Game/Actor/RenderComponent.h"

#include <ppl.h>
#include <ppltasks.h>

namespace AIEditor
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

void AIEditorSoundFetcher::PathsInsert(std::set<std::string>& dstPaths,
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

void  AIEditorSoundFetcher::FetchSounds(const std::string& name, std::set<std::string>& dstPaths)
{
	if (mFetched.count(name))
		return;

	mFetched.insert(name);

	std::string soundBase = "art/quake/audio";
	PathsInsert(dstPaths, soundBase, name);
}

//========================================================================
//
// QuakeAIEditorUI implementation
//
//========================================================================

QuakeAIEditorUI::QuakeAIEditorUI(const QuakeAIEditorView* view) : mAIEditorView(view)
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

QuakeAIEditorUI::~QuakeAIEditorUI()
{ 

}

bool QuakeAIEditorUI::OnInit()
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

	mFlags = QuakeAIEditorUI::Flags();
	mFlags.showDebug = Settings::Get()->GetBool("show_debug");

	return true;
}

void QuakeAIEditorUI::Update(
	const AIEditor::RunStats& stats, const std::shared_ptr<CameraNode> camera,
	const std::shared_ptr<UIChatConsole> chatConsole, float dTime)
{
	Vector2<unsigned int> screensize = Renderer::Get()->GetScreenSize();

	if (mFlags.showDebug)
	{
		static float drawTimeAvg = 0;
		drawTimeAvg = drawTimeAvg * 0.95f + stats.drawTime * 0.05f;
		unsigned short fps = (unsigned short)(1.f / stats.dTimeJitter.avg);

		std::ostringstream os(std::ios_base::binary);
		os << std::fixed
			<< "Quake " << " | FPS: " << fps
			<< std::setprecision(0)
			<< " | drawTime: " << drawTimeAvg << "ms"
			<< std::setprecision(1)
			<< " | dtime jitter: "
			<< (stats.dTimeJitter.maxFraction * 100.0) << "%"
			<< std::setprecision(1);
		//<< std::setprecision(2)
		//<< " | RTT: " << (client->getRTT() * 1000.0f) << "ms";
		mText->SetText(ToWideString(os.str()).c_str());
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

void QuakeAIEditorUI::ShowTranslatedStatusText(const char* str)
{
	ShowStatusText(ToWideString(str));
}

void QuakeAIEditorUI::SetChatText(const EnrichedString& chatText, unsigned int recentChatCount)
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

void QuakeAIEditorUI::UpdateProfiler()
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

void QuakeAIEditorUI::ToggleChat()
{
	mFlags.showChat = !mFlags.showChat;
	if (mFlags.showChat)
		ShowTranslatedStatusText("Chat shown");
	else
		ShowTranslatedStatusText("Chat hidden");
}

void QuakeAIEditorUI::ToggleHud()
{
	mFlags.showHud = !mFlags.showHud;
	if (mFlags.showHud)
		ShowTranslatedStatusText("HUD shown");
	else
		ShowTranslatedStatusText("HUD hidden");
}

void QuakeAIEditorUI::ToggleProfiler()
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
void QuakeAIEditorUI::ShowOverlayMessage(const std::wstring& text,
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

bool QuakeAIEditorUI::IsMenuActive()
{
	return mIsMenuActive;
}

void QuakeAIEditorUI::SetMenuActive(bool active)
{
	mIsMenuActive = active;
}

bool QuakeAIEditorUI::OnRestore()
{
	return BaseUI::OnRestore();
}

bool QuakeAIEditorUI::OnRender(double time, float elapsedTime)
{
	TimeTaker ttDraw("Draw scene");

	AIEditor::RunStats stats = { 0 };
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
	
	if (mAIEditorView->mCamera->GetTarget())
	{
		std::shared_ptr<PlayerActor> player(std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(mAIEditorView->mCamera->GetTarget()->GetId()).lock()));
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

bool QuakeAIEditorUI::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc( evt );
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool QuakeAIEditorUI::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_UI_EVENT)
	{
		int id = evt.mUIEvent.mCaller->GetID();
	}

	return false;
}


//========================================================================
//
// QuakeAIEditorView Implementation
//
//========================================================================



void AIEditorSettings::ReadGlobalSettings()
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

void QuakeAIEditorView::SettingsChangedCallback(const std::string& name, void* data)
{
	((AIEditorSettings*)data)->ReadGlobalSettings();
}


//
// QuakeAIEditorView::QuakeAIEditorView	- Chapter 19, page 724
//
QuakeAIEditorView::QuakeAIEditorView() : 
	HumanView(), mGraphNode(nullptr), mPathNode(nullptr)
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
			&QuakeAIEditorView::SettingsChangedCallback, &mSettings);
	}
}


QuakeAIEditorView::~QuakeAIEditorView()
{
    RemoveAllDelegates();

	// mSettings becomes invalid, remove callbacks
	for (const std::string& name : mSettings.settingNames)
	{
		Settings::Get()->DeregisterChangedCallback(name,
			&QuakeAIEditorView::SettingsChangedCallback, &mSettings);
	}

	Shutdown();
}

//
// QuakeAIEditorView::OnMsgProc				- Chapter 19, page 727
//
bool QuakeAIEditorView::OnMsgProc( const Event& evt )
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
// QuakeAIEditorView::RenderText				- Chapter 19, page 727
//
void QuakeAIEditorView::RenderText()
{
	HumanView::RenderText();
}

//
// QuakeAIEditorView::OnRender
//
void QuakeAIEditorView::OnRender(double time, float elapsedTime)
{
	// Drawing begins
	Renderer::Get()->SetClearColor(SColor(255, 140, 186, 250));

	HumanView::OnRender(time, elapsedTime);
}

//
// QuakeAIEditorView::OnUpdate				- Chapter 19, page 730
//
void QuakeAIEditorView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
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

void QuakeAIEditorView::Step(float dTime)
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
// QuakeAIEditorView::OnAttach				- Chapter 19, page 731
//
void QuakeAIEditorView::OnAttach(GameViewId vid, ActorId aid)
{
	HumanView::OnAttach(vid, aid);
}

bool QuakeAIEditorView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
	if (!HumanView::LoadGameDelegate(pLevelData))
		return false;

    mUI.reset(new QuakeAIEditorUI(this));
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

	mStats = { 0 };
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
void QuakeAIEditorView::UpdateProfilers(const AIEditor::RunStats& stats, 
	const AIEditor::FpsControl& updateTimes, float dTime)
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
	Profiling->GraphAdd("Time update [ms]", (float)(updateTimes.busyTime - stats.drawTime));

	Profiling->GraphAdd("Sleep [ms]", (float)updateTimes.sleepTime);
	Profiling->GraphAdd("FPS", 1.f / dTime);
}

void QuakeAIEditorView::UpdateStats(AIEditor::RunStats* stats, 
	const AIEditor::FpsControl& updateTimes, float dTime)
{
	float jitter;
	AIEditor::Jitter* jp;

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

bool QuakeAIEditorView::GetGameContent()
{
	ClearInput();

	AIEditor::FpsControl fpsControl = { 0 };
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

		AIEditor::DrawLoadScreen(text, mUI, mVisual, mBlendState, (int)progress);
	}

	return true;
}

void QuakeAIEditorView::AfterContentReceived()
{
	LogInformation("QuakeAIEditorView::AfterContentReceived() started");
	LogAssert(mMediaReceived, "no media received"); // pre-condition

	std::wstring text = L"Loading textures...";

	// Clear cached pre-scaled 2D GUI images, as this cache
	// might have images with the same name but different
	// content from previous sessions.
	mUI->GetSkin()->ClearTextureCache();

	// Rebuild inherited images and recreate textures
	LogInformation("- Rebuilding images and textures");
	AIEditor::DrawLoadScreen(text, mUI, mVisual, mBlendState, 70);

	// Rebuild shaders
	LogInformation("- Rebuilding shaders");
	text = L"Rebuilding shaders...";
	AIEditor::DrawLoadScreen(text, mUI, mVisual, mBlendState, 71);

	// Update node aliases
	LogInformation("- Updating node aliases");
	text = L"Initializing nodes...";
	AIEditor::DrawLoadScreen(text, mUI, mVisual, mBlendState, 72);

	// Update node textures and assign shaders to each tile
	LogInformation("- Updating node textures");
	AIEditor::TextureUpdateArgs textureUpdateArgs;
	textureUpdateArgs.ui = mUI;
	textureUpdateArgs.scene = mScene.get();
	textureUpdateArgs.lastTimeMs = Timer::GetRealTime();
	textureUpdateArgs.lastPercent = 0;
	textureUpdateArgs.visual = mVisual;
	textureUpdateArgs.blendState = mBlendState;
	textureUpdateArgs.textBase = L"Initializing nodes";

	text = L"Done!";
	AIEditor::DrawLoadScreen(text, mUI, mVisual, mBlendState, 100);
	LogInformation("QuakeAIEditorView::afterContentReceived() done");

	mGameState = BGS_RUNNING;
}


/****************************************************************************
 Input handling
 ****************************************************************************/
void QuakeAIEditorView::ProcessUserInput(float dTime)
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

void QuakeAIEditorView::ProcessKeyInput()
{
	if (CancelPressed())
	{
		if (!mUI->IsMenuActive() && !mUI->mChatConsole->IsOpenInhibited())
			ShowPauseMenu();
	}
	else if (WasKeyDown(KeyType::DIG))
	{
		if (mGraphNode && mGraphNode->IsVisible())
		{
			// Use the scene graph picking system to generate lines
			Vector4<float> start, end, direction;
			int viewX, viewY, viewW, viewH;
			Renderer::Get()->GetViewport(viewX, viewY, viewW, viewH);
			Vector2<unsigned int> pos = System::Get()->GetCursorControl()->GetPosition();
			pos[1] = viewH - pos[1];
			if (mCamera->Get()->GetPickLine(viewX, viewY, viewW, viewH, pos[0], pos[1], start, direction))
			{
				float extent = 2.0f * mCamera->Get()->GetDMax();
				end = start + extent * direction;
				printf("\n start %f %f %f", start[0], start[1], start[2]);
				printf("\n ray dir %f %f %f, ray end %f %f %f",
					direction[0], direction[1], direction[2], end[0], end[1], end[2]);

				for (auto& clusterBB : mClustersBB)
				{
					if (clusterBB.second.Intersect(HProject(start), HProject(direction)))
					{
						printf("\n cluster %i", clusterBB.first);
						mSelectedClusters[clusterBB.first] = 0;
					}
				}

				if (mUI->mFormName == "MAP")
				{
					mGraphNode->GenerateMesh(mSelectedClusters, mMap);
					ShowMap(mSelectedClusters);
				}
				else if (mUI->mFormName == "PATHING")
				{
					mGraphNode->GenerateMesh(mSelectedClusters, mPathingMap);
					ShowPathingMap(mSelectedClusters);
				}
				else if (mUI->mFormName == "EDIT_MAP")
				{
					mGraphNode->GenerateMesh(mSelectedClusters, mPathingMap);
					EditMap(mSelectedClusters);
				}
				else if (mUI->mFormName == "EDIT_PATHING")
				{
					mGraphNode->GenerateMesh(mSelectedClusters, mPathingMap);
					EditPathingMap(mSelectedClusters);
				}
			}
		}
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
					std::shared_ptr<QuakeAIEditorView> pHumanView =
						std::static_pointer_cast<QuakeAIEditorView, BaseGameView>(pView);
					if (pHumanView->GetActorId() != mPlayer->GetId())
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

void QuakeAIEditorView::UpdateFrame(AIEditor::RunStats* stats, float dTime)
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


bool QuakeAIEditorView::LoadMedia(const std::string& filePath, bool fromMediaPush)
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

std::string QuakeAIEditorView::GetModStoragePath() const
{
	std::string currentDir = ToString(FileSystem::Get()->GetWorkingDirectory());
	return currentDir + "/mod_storage";
}

void QuakeAIEditorView::SetControlledActor(ActorId actorId)
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


void QuakeAIEditorView::ShowFormDelegate(BaseEventDataPtr pEventData)
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


void QuakeAIEditorView::InitChatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataInitChat> pCastEventData =
		std::static_pointer_cast<EventDataInitChat>(pEventData);

	mUI->mChatConsole->SetChat(pCastEventData->GetChat());
}


void QuakeAIEditorView::UpdateChatDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataUpdateChat> pCastEventData =
		std::static_pointer_cast<EventDataUpdateChat>(pEventData);

	// Display all messages in a static text element
	mUI->SetChatText(pCastEventData->GetChat(), pCastEventData->GetLineCount());
}

void QuakeAIEditorView::HandlePlaySoundAtDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataPlaySoundAt> pCastEventData =
		std::static_pointer_cast<EventDataPlaySoundAt>(pEventData);

	mSoundMgr->PlaySoundAt(pCastEventData->GetSoundName(), pCastEventData->IsLoop(),
		pCastEventData->GetGain(), pCastEventData->GetPosition(), pCastEventData->GetPitch());
}

void QuakeAIEditorView::HandlePlaySoundDelegate(BaseEventDataPtr pEventData)
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

void QuakeAIEditorView::HandleStopSoundDelegate(BaseEventDataPtr pEventData)
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

void QuakeAIEditorView::HandleFadeSoundDelegate(BaseEventDataPtr pEventData)
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

void QuakeAIEditorView::ChangeVolumeDelegate(BaseEventDataPtr pEventData)
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

void QuakeAIEditorView::HandleMediaDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataHandleMedia> pCastEventData =
		std::static_pointer_cast<EventDataHandleMedia>(pEventData);

	// Check media cache
	for (auto const& media : pCastEventData->GetMedia())
		LoadMedia(ToString(media.second));

	mMediaReceived = true;
}

void QuakeAIEditorView::ChangeMenuDelegate(BaseEventDataPtr pEventData)
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

void QuakeAIEditorView::GameplayUiUpdateDelegate(BaseEventDataPtr pEventData)
{
    std::shared_ptr<EventDataGameplayUIUpdate> pCastEventData =
		std::static_pointer_cast<EventDataGameplayUIUpdate>(pEventData);
    if (!pCastEventData->GetUiString().empty())
        mGameplayText = pCastEventData->GetUiString();
    else
		mGameplayText.clear();
}

void QuakeAIEditorView::FireWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFireWeapon> pCastEventData =
		std::static_pointer_cast<EventDataFireWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::ChangeWeaponDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataChangeWeapon> pCastEventData =
		std::static_pointer_cast<EventDataChangeWeapon>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::DeadActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataDeadActor> pCastEventData =
		std::static_pointer_cast<EventDataDeadActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::SpawnActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSpawnActor> pCastEventData =
		std::static_pointer_cast<EventDataSpawnActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::JumpActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataJumpActor> pCastEventData =
		std::static_pointer_cast<EventDataJumpActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::MoveActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataMoveActor> pCastEventData =
		std::static_pointer_cast<EventDataMoveActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
		GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::FallActorDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataFallActor> pCastEventData =
		std::static_pointer_cast<EventDataFallActor>(pEventData);

	ActorId actorId = pCastEventData->GetId();
	std::shared_ptr<PlayerActor> pPlayerActor(
		std::dynamic_pointer_cast<PlayerActor>(
			GameLogic::Get()->GetActor(actorId).lock()));
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

void QuakeAIEditorView::RotateActorDelegate(BaseEventDataPtr pEventData)
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

void QuakeAIEditorView::ClearMapDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataClear> pCastEventData =
		std::static_pointer_cast<EventDataClear>(pEventData);

	mSelectedClusters.clear();

	if (mGraphNode)
	{
		std::map<unsigned short, unsigned short> selectedClusters;

		mGraphNode->SetVisible(true);
		mGraphNode->GenerateMesh(selectedClusters, mPathingMap);

		const ClusterMap& clusters = mPathingMap->GetClusters();
		for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
		{
			Cluster* cluster = (*it).second;
			selectedClusters[cluster->GetId()] = 0;
		}
		EditMap(selectedClusters);
	}

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::RemoveArcTypeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataRemoveArcType> pCastEventData =
		std::static_pointer_cast<EventDataRemoveArcType>(pEventData);

	if (mPathingMap)
	{
		PathingArc* pArc = mPathingMap->FindArc(pCastEventData->GetId());
		if (pArc)
		{
			PathingNode* pNode = mPathingMap->FindNode(pArc);
			for (auto& arc : pNode->GetArcs())
				if (arc.second->GetType() == pArc->GetType())
					pNode->RemoveArc(arc.second->GetId());

			EditMap(pNode);
		}
	}
}

void QuakeAIEditorView::RemoveArcDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataRemoveArc> pCastEventData =
		std::static_pointer_cast<EventDataRemoveArc>(pEventData);

	if (mPathingMap)
	{
		PathingArc* pArc = mPathingMap->FindArc(pCastEventData->GetId());
		if (pArc)
		{
			PathingNode* pNode = mPathingMap->FindNode(pArc);
			pNode->RemoveArc(pArc->GetId());

			EditMap(pNode);
		}
	}
}

void QuakeAIEditorView::RemoveNodeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataRemoveNode> pCastEventData =
		std::static_pointer_cast<EventDataRemoveNode>(pEventData);

	if (mPathingMap)
	{
		PathingNode* pNode = mPathingMap->FindNode(pCastEventData->GetId());
		if (pNode)
		{
			mPathingMap->RemoveNode(pNode);

			if (mGraphNode)
			{
				std::map<unsigned short, unsigned short> selectedClusters;
				selectedClusters[pNode->GetCluster()] = 0;

				mGraphNode->SetVisible(true);
				mGraphNode->GenerateMesh(selectedClusters, mPathingMap);

				EditMap(selectedClusters);
			}

			if (mHighlightNode)
				mHighlightNode->SetVisible(false);

			if (mPathNode)
				mPathNode->SetVisible(false);
		}
	}
}

void QuakeAIEditorView::EditMapNodeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataEditMapNode> pCastEventData =
		std::static_pointer_cast<EventDataEditMapNode>(pEventData);

	if (mPathingMap)
	{
		PathingNode* pNode = mPathingMap->FindNode(pCastEventData->GetId());
		if (pNode)
		{
			if (mGraphNode)
			{
				std::map<unsigned short, unsigned short> selectedClusters;
				selectedClusters[pNode->GetCluster()] = 0;

				mGraphNode->GenerateMesh(selectedClusters, mPathingMap);
				mGraphNode->SetVisible(true);

				EditMap(pNode);
			}

			if (!mHighlightNode)
			{
				std::shared_ptr<ResHandle>& resHandle =
					ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
				if (resHandle)
				{
					const std::shared_ptr<ImageResourceExtraData>& extra =
						std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
					extra->GetImage()->AutogenerateMipmaps();

					Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
					mHighlightNode = mScene->AddBoxNode(0, extra->GetImage(), { 1.0f, 1.0f }, size);
				}
			}

			if (mGraphNode)
			{
				for (unsigned int i = 0; i < mHighlightNode->GetVisualCount(); ++i)
				{
					std::shared_ptr<Visual> visual = mHighlightNode->GetVisual(i);
					if (visual)
					{
						std::shared_ptr<PointLightTextureEffect> textureEffect =
							std::dynamic_pointer_cast<PointLightTextureEffect>(visual->GetEffect());
						std::shared_ptr<Material> material = textureEffect->GetMaterial();
						material->mDiffuse = mGraphNode->GetClusterColor(pNode->GetCluster());
						material->mAmbient = mGraphNode->GetClusterColor(pNode->GetCluster());
						textureEffect->UpdateMaterialConstant();
					}
				}
			}

			mHighlightNode->GetRelativeTransform().SetTranslation(pNode->GetPosition());
			mHighlightNode->UpdateAbsoluteTransform();
			mHighlightNode->SetVisible(true);
		}
	}
}

void QuakeAIEditorView::ShowMapNodeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowMapNode> pCastEventData =
		std::static_pointer_cast<EventDataShowMapNode>(pEventData);

	if (mMap)
	{
		PathingNode* pNode = mMap->FindNode(pCastEventData->GetId());
		if (pNode)
		{
			if (mGraphNode)
			{
				std::map<unsigned short, unsigned short> selectedClusters;
				selectedClusters[pNode->GetCluster()] = 0;

				mGraphNode->GenerateMesh(selectedClusters, mMap);
				mGraphNode->SetVisible(true);

				ShowMap(pNode);
			}

			if (!mHighlightNode)
			{
				std::shared_ptr<ResHandle>& resHandle =
					ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
				if (resHandle)
				{
					const std::shared_ptr<ImageResourceExtraData>& extra =
						std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
					extra->GetImage()->AutogenerateMipmaps();

					Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
					mHighlightNode = mScene->AddBoxNode(0, extra->GetImage(), { 1.0f, 1.0f }, size);
				}
			}

			if (mGraphNode)
			{
				for (unsigned int i = 0; i < mHighlightNode->GetVisualCount(); ++i)
				{
					std::shared_ptr<Visual> visual = mHighlightNode->GetVisual(i);
					if (visual)
					{
						std::shared_ptr<PointLightTextureEffect> textureEffect =
							std::dynamic_pointer_cast<PointLightTextureEffect>(visual->GetEffect());
						std::shared_ptr<Material> material = textureEffect->GetMaterial();
						material->mDiffuse = mGraphNode->GetClusterColor(pNode->GetCluster());
						material->mAmbient = mGraphNode->GetClusterColor(pNode->GetCluster());
						textureEffect->UpdateMaterialConstant();
					}
				}
			}

			mHighlightNode->GetRelativeTransform().SetTranslation(pNode->GetPosition());
			mHighlightNode->UpdateAbsoluteTransform();
			mHighlightNode->SetVisible(true);
		}
	}
}

void QuakeAIEditorView::HighlightNodeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataHighlightNode> pCastEventData =
		std::static_pointer_cast<EventDataHighlightNode>(pEventData);

	PathingNode* pNode = 
		mPathingMap ? mPathingMap->FindNode(pCastEventData->GetId()) : nullptr;
	if (!pNode)
		pNode = mMap ? mMap->FindNode(pCastEventData->GetId()) : nullptr;
	if (!pNode)
		return;
		
	if (!mHighlightNode)
	{
		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
			mHighlightNode = mScene->AddBoxNode(0, extra->GetImage(), { 1.0f, 1.0f }, size);
		}
	}

	if (mGraphNode)
	{
		for (unsigned int i = 0; i < mHighlightNode->GetVisualCount(); ++i)
		{
			std::shared_ptr<Visual> visual = mHighlightNode->GetVisual(i);
			if (visual)
			{
				std::shared_ptr<PointLightTextureEffect> textureEffect =
					std::dynamic_pointer_cast<PointLightTextureEffect>(visual->GetEffect());
				std::shared_ptr<Material> material = textureEffect->GetMaterial();
				material->mDiffuse = mGraphNode->GetClusterColor(pNode->GetCluster());
				material->mAmbient = mGraphNode->GetClusterColor(pNode->GetCluster());
				textureEffect->UpdateMaterialConstant();
			}
		}
	}

	mHighlightNode->GetRelativeTransform().SetTranslation(pNode->GetPosition());
	mHighlightNode->UpdateAbsoluteTransform();
	mHighlightNode->SetVisible(true);
}

void QuakeAIEditorView::SimulateExploringDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSimulateExploring> pCastEventData =
		std::static_pointer_cast<EventDataSimulateExploring>(pEventData);

	if (mGraphNode)
		mGraphNode->SetVisible(false);

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	int skipArc = -1;// Randomizer::Rand() % 2 ? AT_JUMP : -1;
	PathingNode* pStartNode = nullptr;
	PathingNode* pEndNode = pCastEventData->GetNodeId() != -1 ?
		mPathingMap->FindNode(pCastEventData->GetNodeId()) : mPathingMap->FindRandomNode();
	std::shared_ptr<Actor> pActor = GameLogic::Get()->GetActor(pCastEventData->GetActorId()).lock();
	if (pActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicComponent(
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (pPhysicComponent)
			pStartNode = mPathingMap->FindClosestNode(pPhysicComponent->GetTransform().GetTranslation());
	}

	NodePlan playerPlan;
	PathPlan* pathPlan = mPathingMap->FindPath(pStartNode, pEndNode, skipArc);
	if (pathPlan)
	{
		playerPlan.ResetPathPlan(pathPlan->GetArcs());
		playerPlan.node = pStartNode;
		delete pathPlan;
	}

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
	std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
	if (playerPlan.node)
	{
		PlayerView playerView;
		aiManager->GetPlayerView(pCastEventData->GetActorId(), playerView);

		playerView.isUpdated = true;
		playerView.simulation.plan = playerPlan;
		playerView.simulation.plan.id = aiManager->GetNewPlanID();
		aiManager->UpdatePlayerSimulationView(pCastEventData->GetActorId(), playerView);

		aiView->SetPathingGraph(mPathingMap);
	}

	std::vector<Vector3<float>> nodes;
	for (auto const& arc : playerPlan.path)
	{
		PathingTransition* transition = arc->GetTransition();
		if (transition)
			for (auto const& position : transition->GetPositions())
				nodes.push_back(position);
	}
	if (nodes.empty())
		return;

	if (!mPathNode)
	{
		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size{ 2.5f, 2.5f, 2.5f };
			mPathNode = std::make_shared<PathNode>(GameLogic::Get()->GetNewActorID(),
				&mScene->GetPVWUpdater(), extra->GetImage(), size);
			mScene->AddSceneNode(mPathNode->GetId(), mPathNode);
		}
	}

	if (mPathNode)
	{
		mPathNode->SetVisible(true);
		mPathNode->GenerateMesh(nodes);
	}

	mPlayer = mScene->GetSceneNode(aiView->GetActorId());

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

void QuakeAIEditorView::CreatePathingGraphDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataCreatePathing> pCastEventData =
		std::static_pointer_cast<EventDataCreatePathing>(pEventData);

	if (mGraphNode)
		mGraphNode->SetVisible(false);

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	aiManager->CreatePathingMap(pCastEventData->GetActorId(), mCreatedNodes, mSelectedClusters, mPathingMap);
	if (mCreatedNodes.empty())
		return;

	std::vector<Vector3<float>> nodes;
	PathingNode* pNode = mCreatedNodes.back();
	if (pNode)
	{
		for (auto const& pArc : pNode->GetArcs())
		{
			PathingTransition* pTransition = pArc.second->GetTransition();
			if (pTransition)
				for (auto const& position : pTransition->GetPositions())
					nodes.push_back(position);
		}
	}
	if (nodes.empty())
		return;

	if (!mPathNode)
	{
		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size{ 2.5f, 2.5f, 2.5f };
			mPathNode = std::make_shared<PathNode>(GameLogic::Get()->GetNewActorID(),
				&mScene->GetPVWUpdater(), extra->GetImage(), size);
			mScene->AddSceneNode(mPathNode->GetId(), mPathNode);
		}
	}

	if (mPathNode)
	{
		mPathNode->SetVisible(true);
		mPathNode->GenerateMesh(nodes);
	}
}

void QuakeAIEditorView::CreatePathingNodeDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataCreatePathingNode> pCastEventData =
		std::static_pointer_cast<EventDataCreatePathingNode>(pEventData);

	if (mGraphNode)
		mGraphNode->SetVisible(false);

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	PathingNode* pNode = aiManager->CreatePathingNode(pCastEventData->GetActorId(), mPathingMap);

	if (pNode)
	{
		mCreatedNodes.push_back(pNode);

		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
			std::shared_ptr<Node> boxNode = mScene->AddBoxNode(
				0, extra->GetImage(), { 1.0f, 1.0f }, size, GameLogic::Get()->GetLastActorID() + pNode->GetId());
			boxNode->GetRelativeTransform().SetTranslation(pNode->GetPosition());
			boxNode->UpdateAbsoluteTransform();
			boxNode->SetVisible(true);
		}
	}
}

void QuakeAIEditorView::SimulatePathingDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSimulatePathing> pCastEventData =
		std::static_pointer_cast<EventDataSimulatePathing>(pEventData);

	if (mGraphNode)
		mGraphNode->SetVisible(false);

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	NodePlan playerPlan;
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	aiManager->CreatePathing(pCastEventData->GetActorId(), playerPlan);

	std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
	std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
	aiView->SetPathingGraph(aiManager->GetPathingGraph());

	if (playerPlan.node)
	{
		PlayerView playerView; 
		aiManager->GetPlayerView(pCastEventData->GetActorId(), playerView);

		playerView.isUpdated = true;
		playerView.simulation.plan = playerPlan;
		playerView.simulation.plan.id = aiManager->GetNewPlanID();
		aiManager->UpdatePlayerSimulationView(pCastEventData->GetActorId(), playerView);
	}

	std::vector<Vector3<float>> nodes;
	for (auto const& pArc : playerPlan.path)
	{
		PathingTransition* pTransition = pArc->GetTransition();
		if (pTransition)
			for (auto const& position : pTransition->GetPositions())
				nodes.push_back(position);
	}
	if (nodes.empty())
		return;

	if (!mPathNode)
	{
		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size{ 2.5f, 2.5f, 2.5f };
			mPathNode = std::make_shared<PathNode>(GameLogic::Get()->GetNewActorID(),
				&mScene->GetPVWUpdater(), extra->GetImage(), size);
			mScene->AddSceneNode(mPathNode->GetId(), mPathNode);
		}
	}

	if (mPathNode)
	{
		mPathNode->SetVisible(true);
		mPathNode->GenerateMesh(nodes);
	}
}

void QuakeAIEditorView::EditPathingGraphDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataEditPathing> pCastEventData = 
		std::static_pointer_cast<EventDataEditPathing>(pEventData);

	mSelectedClusters.clear();

	if (!mPathingMap)
	{
		mPathingMap = std::make_shared<PathingGraph>();
		std::string levelPath = "ai/quake/" + Settings::Get()->Get("selected_world") + "/map.bin";
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		aiManager->LoadPathingMap(ToWideString(FileSystem::Get()->GetPath(levelPath.c_str()).c_str()), mPathingMap);

		for (auto& node : mPathingMap->GetNodes())
			if (!mPathingMap->FindCluster(node.second->GetCluster()))
				mCreatedNodes.push_back(node.second);
	}

	if (!mGraphNode)
	{
		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
			Vector3<float> halfSize = size / 2.f;

			ActorId graphId = GameLogic::Get()->GetNewActorID();
			mGraphNode = std::make_shared<GraphNode>(graphId,
				&mScene->GetPVWUpdater(), extra->GetImage(), size, mPathingMap);
			mScene->AddSceneNode(graphId, mGraphNode);
			mGraphNode->GenerateMesh(std::map<unsigned short, unsigned short>(), mPathingMap);

			mClustersBB.clear();
			const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
			for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
			{
				PathingNode* node = (*it).second;

				if (mClustersBB.find(node->GetCluster()) == mClustersBB.end())
				{
					mClustersBB[node->GetCluster()].mMinEdge = node->GetPosition() - halfSize;
					mClustersBB[node->GetCluster()].mMaxEdge = node->GetPosition() + halfSize;
				}
				else
				{
					mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() - halfSize);
					mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() + halfSize);
				}
			}
		}
	}
	else
	{
		mGraphNode->SetVisible(true);
		mGraphNode->GenerateMesh(std::map<unsigned short, unsigned short>(), mPathingMap);
	}

	std::map<unsigned short, unsigned short> selectedClusters;
	const ClusterMap& clusters = mPathingMap->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* cluster = (*it).second;
		selectedClusters[cluster->GetId()] = 0;
	}
	EditPathingMap(selectedClusters);

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::CreatePathingMapDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataCreatePathingMap> pCastEventData =
		std::static_pointer_cast<EventDataCreatePathingMap>(pEventData);

	mSelectedClusters.clear();

	if (!mPathingMap)
	{
		mPathingMap = std::make_shared<PathingGraph>();
		std::string filePath = "ai/quake/" + Settings::Get()->Get("selected_world") + "/map.bin";
		if (!FileSystem::Get()->ExistFile(ToWideString(filePath)))
		{
			std::string fullFilePath = ToString(FileSystem::Get()->GetWorkingDirectory().c_str()) + "/../../assets/" + filePath;
			if (!FileSystem::Get()->SafeWriteToFile(fullFilePath, ""))
			{
				LogError("Error creating map file: \"" + filePath + "\"");
				return;
			}

			QuakeLogic* game = dynamic_cast<QuakeLogic*>(GameLogic::Get());
			QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(game->GetAIManager());

			std::vector<std::shared_ptr<Actor>> actors;
			game->GetAmmoActors(actors);
			game->GetWeaponActors(actors);
			game->GetHealthActors(actors);
			game->GetArmorActors(actors);
			for (std::shared_ptr<Actor> actor : actors)
			{
				std::shared_ptr<TransformComponent> pTransformComponent(
					actor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
				if (pTransformComponent)
				{ 
					PathingNode* pNode = aiManager->CreatePathingNode(
						pCastEventData->GetActorId(), pTransformComponent->GetPosition(), mPathingMap);
					mCreatedNodes.push_back(pNode);
				}
			}

			actors.clear();
			game->GetTargetActors(actors);
			for (std::shared_ptr<Actor> actor : actors)
			{
				if (actor->GetComponent<LocationTarget>(LocationTarget::Name).lock())
				{
					std::shared_ptr<TransformComponent> pTransformComponent(
						actor->GetComponent<TransformComponent>(TransformComponent::Name).lock());
					if (pTransformComponent)
					{
						PathingNode* pNode = aiManager->CreatePathingNode(
							pCastEventData->GetActorId(), pTransformComponent->GetPosition(), mPathingMap);
						mCreatedNodes.push_back(pNode);
					}
				}
			}
		}
		else
		{
			QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
			aiManager->LoadPathingMap(ToWideString(FileSystem::Get()->GetPath(filePath.c_str()).c_str()), mPathingMap);
			for (auto& node : mPathingMap->GetNodes())
				if (!mPathingMap->FindCluster(node.second->GetCluster()))
					mCreatedNodes.push_back(node.second);
		}

		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
			for (PathingNode* pNode : mCreatedNodes)
			{
				GameLogic::Get()->GetNewActorID();
				std::shared_ptr<Node> boxNode = mScene->AddBoxNode(
					0, extra->GetImage(), { 1.0f, 1.0f }, size, GameLogic::Get()->GetLastActorID() + pNode->GetId());
				boxNode->GetRelativeTransform().SetTranslation(pNode->GetPosition());
				boxNode->UpdateAbsoluteTransform();
				boxNode->SetVisible(true);
			}
		}
	}

	CreatePathingMap();

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::ShowPathingGraphDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowPathing> pCastEventData =
		std::static_pointer_cast<EventDataShowPathing>(pEventData);

	mSelectedClusters.clear();

	if (mGraphNode)
	{
		std::map<unsigned short, unsigned short> selectedClusters;

		mGraphNode->SetVisible(true);
		mGraphNode->GenerateMesh(selectedClusters, mPathingMap);

		const ClusterMap& clusters = mPathingMap->GetClusters();
		for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
		{
			Cluster* cluster = (*it).second;
			selectedClusters[cluster->GetId()] = 0;
		}
		ShowPathingMap(selectedClusters);
	}

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::ShowNodeVisibilityDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataNodeVisibility> pCastEventData =
		std::static_pointer_cast<EventDataNodeVisibility>(pEventData);

	PathingNode* pathingNode = mMap->FindNode(pCastEventData->GetId());
	if (pathingNode && mGraphNode)
	{
		mGraphNode->SetVisible(true);
		mGraphNode->GenerateMesh(pathingNode->GetVisibileNodes(), mMap);

		std::map<unsigned short, unsigned short> selectedClusters;
		const ClusterMap& clusters = mMap->GetClusters();
		for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
		{
			Cluster* cluster = (*it).second;
			selectedClusters[cluster->GetId()] = 0;
		}
		ShowMap(selectedClusters);
	}
}

void QuakeAIEditorView::ShowNodeConnectionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataNodeConnection> pCastEventData =
		std::static_pointer_cast<EventDataNodeConnection>(pEventData);

	PathingNode* pathingNode = nullptr;
	if (mUI->mFormName == "MAP")
		pathingNode = mMap->FindNode(pCastEventData->GetId());
	else
		pathingNode = mPathingMap->FindNode(pCastEventData->GetId());

	if (pathingNode)
	{
		std::vector<Vector3<float>> nodes;
		for (auto const& arc : pathingNode->GetArcs())
		{
			PathingArc* pArc = arc.second;
			PathingTransition* transition = pArc->GetTransition();
			if (transition)
				for (auto const& position : transition->GetPositions())
					nodes.push_back(position);
		}
		if (nodes.empty())
			return;

		if (mGraphNode)
			mGraphNode->SetVisible(false);

		if (!mPathNode)
		{
			std::shared_ptr<ResHandle>& resHandle =
				ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
			if (resHandle)
			{
				const std::shared_ptr<ImageResourceExtraData>& extra =
					std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
				extra->GetImage()->AutogenerateMipmaps();

				Vector3<float> size{ 2.5f, 2.5f, 2.5f };
				mPathNode = std::make_shared<PathNode>(GameLogic::Get()->GetNewActorID(),
					&mScene->GetPVWUpdater(), extra->GetImage(), size);
				mScene->AddSceneNode(mPathNode->GetId(), mPathNode);
			}
		}

		if (mPathNode)
		{
			mPathNode->SetVisible(true);
			mPathNode->GenerateMesh(nodes);
		}
	}
	else if (!mSelectedClusters.empty())
	{
		mGraphNode->SetVisible(true);
		if (mUI->mFormName == "MAP")
		{
			std::map<unsigned short, unsigned short> selectedClusters;
			unsigned short cluster = mSelectedClusters.begin()->first;

			std::map<PathingCluster*, PathingArcVec> clusterPaths, otherClusterPaths;
			std::map<PathingCluster*, float> clusterPathWeights, otherClusterPathWeights;
			PathingNode* clusterNode = mMap->FindClusterNode(cluster);
			clusterNode->GetClusters(AT_MOVE, 100, clusterPaths, clusterPathWeights);
			clusterNode->GetClusters(AT_JUMP, 100, clusterPaths, clusterPathWeights);
			for (auto& clusterPath : clusterPaths)
				selectedClusters[clusterPath.first->GetTarget()->GetCluster()] = 0;

			mGraphNode->GenerateMesh(selectedClusters, mMap);
		}
		else
		{
			std::map<unsigned short, unsigned short> selectedClusters;
			unsigned short cluster = mSelectedClusters.begin()->first;

			std::map<PathingCluster*, PathingArcVec> clusterPaths, otherClusterPaths;
			std::map<PathingCluster*, float> clusterPathWeights, otherClusterPathWeights;
			PathingNode* clusterNode = mPathingMap->FindClusterNode(cluster);
			clusterNode->GetClusters(AT_MOVE, 100, clusterPaths, clusterPathWeights);
			clusterNode->GetClusters(AT_JUMP, 100, clusterPaths, clusterPathWeights);
			for (auto& clusterPath : clusterPaths)
				selectedClusters[clusterPath.first->GetTarget()->GetCluster()] = 0;

			mGraphNode->GenerateMesh(selectedClusters, mPathingMap);
		}
	}
}

void QuakeAIEditorView::ShowArcConnectionDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataArcConnection> pCastEventData =
		std::static_pointer_cast<EventDataArcConnection>(pEventData);

	PathingArc* pathingArc = nullptr;
	if (mUI->mFormName == "MAP")
	{
		pathingArc = mMap->FindArc(pCastEventData->GetId());
		if (!pathingArc)
			return;
	}
	else
	{
		pathingArc = mPathingMap->FindArc(pCastEventData->GetId());
		if (!pathingArc)
			return;
	}

	std::vector<Vector3<float>> nodes;
	PathingTransition* transition = pathingArc->GetTransition();
	if (transition)
		for (auto const& position : transition->GetPositions())
			nodes.push_back(position);
	if (nodes.empty())
		return;

	if (mGraphNode)
		mGraphNode->SetVisible(false);

	if (!mPathNode)
	{
		std::shared_ptr<ResHandle>& resHandle =
			ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			Vector3<float> size{ 2.5f, 2.5f, 2.5f };
			mPathNode = std::make_shared<PathNode>(GameLogic::Get()->GetNewActorID(),
				&mScene->GetPVWUpdater(), extra->GetImage(), size);
			mScene->AddSceneNode(mPathNode->GetId(), mPathNode);
		}
	}

	if (mPathNode)
	{
		mPathNode->SetVisible(true);
		mPathNode->GenerateMesh(nodes);
	}
}

void QuakeAIEditorView::SaveMapDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataSaveMap> pCastEventData =
		std::static_pointer_cast<EventDataSaveMap>(pEventData);

	if (mPathingMap)
	{
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		aiManager->CreateTransitions(mPathingMap);

		std::string levelPath = "ai/quake/" +
			Settings::Get()->Get("selected_world") + "/map.bin";
		aiManager->SaveGraph(
			FileSystem::Get()->GetPath(levelPath.c_str()), mPathingMap);
	}
}

void QuakeAIEditorView::EditMapDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataEditMap> pCastEventData =
		std::static_pointer_cast<EventDataEditMap>(pEventData);

	if (!mPathingMap)
	{
		mPathingMap = std::make_shared<PathingGraph>();
		std::string levelPath = "ai/quake/" + Settings::Get()->Get("selected_world") + "/map.bin";
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		aiManager->LoadPathingMap(ToWideString(FileSystem::Get()->GetPath(levelPath.c_str()).c_str()), mPathingMap);
	}

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
		Vector3<float> halfSize = size / 2.f;

		ActorId graphId = INVALID_ACTOR_ID;
		if (mGraphNode)
		{
			graphId = mGraphNode->GetId();
			mScene->RemoveSceneNode(graphId);
		}
		else graphId = GameLogic::Get()->GetNewActorID();
		mGraphNode = std::make_shared<GraphNode>(graphId,
			&mScene->GetPVWUpdater(), extra->GetImage(), size, mPathingMap);
		mScene->AddSceneNode(graphId, mGraphNode);
		mGraphNode->GenerateMesh(std::map<unsigned short, unsigned short>(), mPathingMap);

		mClustersBB.clear();
		const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
		for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
		{
			PathingNode* node = (*it).second;
			if (mClustersBB.find(node->GetCluster()) == mClustersBB.end())
			{
				mClustersBB[node->GetCluster()].mMinEdge = node->GetPosition() - halfSize;
				mClustersBB[node->GetCluster()].mMaxEdge = node->GetPosition() + halfSize;
			}
			else
			{
				mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() - halfSize);
				mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() + halfSize);
			}
		}
	}

	std::map<unsigned short, unsigned short> selectedClusters;
	const ClusterMap& clusters = mPathingMap->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* cluster = (*it).second;
		selectedClusters[cluster->GetId()] = 0;
	}
	EditMap(selectedClusters, pCastEventData->GetFilter());

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::ShowMapDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataShowMap> pCastEventData = 
		std::static_pointer_cast<EventDataShowMap>(pEventData);

	mSelectedClusters.clear();

	if (!mMap)
	{
		mMap = std::make_shared<PathingGraph>();
		std::string levelPath = "ai/quake/" + Settings::Get()->Get("selected_world") + "/map.bin";
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		aiManager->LoadGraph(ToWideString(FileSystem::Get()->GetPath(levelPath.c_str()).c_str()), mMap);
	}

	std::shared_ptr<ResHandle>& resHandle = ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
		Vector3<float> halfSize = size / 2.f;

		ActorId graphId = INVALID_ACTOR_ID;
		if (mGraphNode)
		{
			graphId = mGraphNode->GetId();
			mScene->RemoveSceneNode(graphId);
		}
		else graphId = GameLogic::Get()->GetNewActorID();
		mGraphNode = std::make_shared<GraphNode>(graphId,
			&mScene->GetPVWUpdater(), extra->GetImage(), size, mMap);
		mScene->AddSceneNode(graphId, mGraphNode);
		mGraphNode->GenerateMesh(std::map<unsigned short, unsigned short>(), mMap);

		mClustersBB.clear();
		const PathingNodeMap& pathingNodes = mMap->GetNodes();
		for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
		{
			PathingNode* node = (*it).second;

			if (mClustersBB.find(node->GetCluster()) == mClustersBB.end())
			{
				mClustersBB[node->GetCluster()].mMinEdge = node->GetPosition() - halfSize;
				mClustersBB[node->GetCluster()].mMaxEdge = node->GetPosition() + halfSize;
			}
			else
			{
				mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() - halfSize);
				mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() + halfSize);
			}
		}
	}

	std::map<unsigned short, unsigned short> selectedClusters;
	const ClusterMap& clusters = mMap->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* cluster = (*it).second;
		selectedClusters[cluster->GetId()] = 0;
	}
	ShowMap(selectedClusters, pCastEventData->GetFilter());

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::CreateMapDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataCreateMap> pCastEventData =
		std::static_pointer_cast<EventDataCreateMap>(pEventData);

	//move the human player outside the map, so it doesn't interfere with the map generation.
	std::shared_ptr<Actor> pActor = GameLogic::Get()->GetActor(mActorId).lock();
	if (pActor)
	{
		std::shared_ptr<PhysicComponent> pPhysicComponent(
			pActor->GetComponent<PhysicComponent>(PhysicComponent::Name).lock());
		if (pPhysicComponent)
		{
			Transform spawnTransform;
			spawnTransform.SetTranslation(Vector3<float>{1115,655,400});
			EventManager::Get()->TriggerEvent(
				std::make_shared<EventDataSpawnActor>(mActorId, spawnTransform));
		}
	}

	std::shared_ptr<BaseGameView> gameView = GameApplication::Get()->GetGameView(GV_AI);
	std::shared_ptr<QuakeAIView> aiView = std::dynamic_pointer_cast<QuakeAIView>(gameView);
	QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
	aiManager->CreatePathingMap(aiView->GetActorId(), mCreatedNodes, mPathingMap);

	//remove sceneboxes
	for (PathingNode* pNode : mCreatedNodes)
		mScene->RemoveSceneNode(GameLogic::Get()->GetLastActorID() + pNode->GetId());
	mCreatedNodes.clear();
}

void QuakeAIEditorView::CreatePathDelegate(BaseEventDataPtr pEventData)
{
	std::shared_ptr<EventDataCreatePath> pCastEventData =
		std::static_pointer_cast<EventDataCreatePath>(pEventData);

	if (!mPathingMap)
	{
		mPathingMap = std::make_shared<PathingGraph>();
		std::string levelPath = "ai/quake/" + Settings::Get()->Get("selected_world") + "/map.bin";
		QuakeAIManager* aiManager = dynamic_cast<QuakeAIManager*>(GameLogic::Get()->GetAIManager());
		aiManager->LoadPathingMap(ToWideString(FileSystem::Get()->GetPath(levelPath.c_str()).c_str()), mPathingMap);
	}

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		Vector3<float> size = Vector3<float>{ 12.f, 12.f, 26.f };
		Vector3<float> halfSize = size / 2.f;

		ActorId graphId = INVALID_ACTOR_ID;
		if (mGraphNode)
		{
			graphId = mGraphNode->GetId();
			mScene->RemoveSceneNode(graphId);
		}
		else graphId = GameLogic::Get()->GetNewActorID();
		mGraphNode = std::make_shared<GraphNode>(graphId,
			&mScene->GetPVWUpdater(), extra->GetImage(), size, mPathingMap);
		mScene->AddSceneNode(graphId, mGraphNode);
		mGraphNode->GenerateMesh(std::map<unsigned short, unsigned short>(), mPathingMap);

		mClustersBB.clear();
		const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
		for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
		{
			PathingNode* node = (*it).second;

			if (mClustersBB.find(node->GetCluster()) == mClustersBB.end())
			{
				mClustersBB[node->GetCluster()].mMinEdge = node->GetPosition() - halfSize;
				mClustersBB[node->GetCluster()].mMaxEdge = node->GetPosition() + halfSize;
			}
			else
			{
				mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() - halfSize);
				mClustersBB[node->GetCluster()].GrowToContain(node->GetPosition() + halfSize);
			}
		}
	}

	std::map<unsigned short, unsigned short> selectedClusters;
	const ClusterMap& clusters = mPathingMap->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* cluster = (*it).second;
		selectedClusters[cluster->GetId()] = 0;
	}
	ShowPathingMap(selectedClusters, pCastEventData->GetFilter());

	if (mHighlightNode)
		mHighlightNode->SetVisible(false);

	if (mPathNode)
		mPathNode->SetVisible(false);
}

void QuakeAIEditorView::RegisterAllDelegates(void)
{
    BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::InitChatDelegate),
		EventDataInitChat::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::UpdateChatDelegate),
		EventDataUpdateChat::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowFormDelegate),
		EventDataShowForm::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::HandlePlaySoundAtDelegate),
		EventDataPlaySoundAt::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::HandlePlaySoundDelegate),
		EventDataPlaySoundType::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::HandleStopSoundDelegate),
		EventDataStopSound::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::HandleFadeSoundDelegate),
		EventDataFadeSound::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ChangeVolumeDelegate),
		EventDataChangeVolume::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ChangeMenuDelegate),
		EventDataChangeMenu::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::HandleMediaDelegate),
		EventDataHandleMedia::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::FireWeaponDelegate),
		EventDataFireWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ChangeWeaponDelegate),
		EventDataChangeWeapon::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::DeadActorDelegate),
		EventDataDeadActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::SpawnActorDelegate),
		EventDataSpawnActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::MoveActorDelegate),
		EventDataMoveActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::FallActorDelegate),
		EventDataFallActor::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::RotateActorDelegate),
		EventDataRotateActor::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::RemoveArcTypeDelegate),
		EventDataRemoveArcType::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::RemoveArcDelegate),
		EventDataRemoveArc::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::RemoveNodeDelegate),
		EventDataRemoveNode::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::HighlightNodeDelegate),
		EventDataHighlightNode::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::EditMapNodeDelegate),
		EventDataEditMapNode::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowMapNodeDelegate),
		EventDataShowMapNode::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ClearMapDelegate),
		EventDataClear::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::EditPathingGraphDelegate),
		EventDataEditPathing::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowPathingGraphDelegate),
		EventDataShowPathing::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathingMapDelegate),
		EventDataCreatePathingMap::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathingNodeDelegate),
		EventDataCreatePathingNode::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathingGraphDelegate),
		EventDataCreatePathing::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::SimulateExploringDelegate),
		EventDataSimulateExploring::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::SimulatePathingDelegate),
		EventDataSimulatePathing::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowNodeVisibilityDelegate),
		EventDataNodeVisibility::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowNodeConnectionDelegate),
		EventDataNodeConnection::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowArcConnectionDelegate),
		EventDataArcConnection::skEventType);

	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::SaveMapDelegate),
		EventDataSaveMap::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::EditMapDelegate),
		EventDataEditMap::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowMapDelegate),
		EventDataShowMap::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::CreateMapDelegate),
		EventDataCreateMap::skEventType);
	pGlobalEventManager->AddListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathDelegate),
		EventDataCreatePath::skEventType);
}

void QuakeAIEditorView::RemoveAllDelegates(void)
{
	BaseEventManager* pGlobalEventManager = BaseEventManager::Get();
    pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::GameplayUiUpdateDelegate), 
		EventDataGameplayUIUpdate::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::InitChatDelegate),
		EventDataInitChat::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::UpdateChatDelegate),
		EventDataUpdateChat::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowFormDelegate),
		EventDataShowForm::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::HandlePlaySoundAtDelegate),
		EventDataPlaySoundAt::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::HandlePlaySoundDelegate),
		EventDataPlaySoundType::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::HandleStopSoundDelegate),
		EventDataStopSound::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::HandleFadeSoundDelegate),
		EventDataFadeSound::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ChangeVolumeDelegate),
		EventDataChangeVolume::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ChangeMenuDelegate),
		EventDataChangeMenu::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::HandleMediaDelegate),
		EventDataHandleMedia::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::FireWeaponDelegate),
		EventDataFireWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ChangeWeaponDelegate),
		EventDataChangeWeapon::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::DeadActorDelegate),
		EventDataDeadActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::SpawnActorDelegate),
		EventDataSpawnActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::JumpActorDelegate),
		EventDataJumpActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::MoveActorDelegate),
		EventDataMoveActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::FallActorDelegate),
		EventDataFallActor::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::RotateActorDelegate),
		EventDataRotateActor::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::RemoveArcTypeDelegate),
		EventDataRemoveArcType::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::RemoveArcDelegate),
		EventDataRemoveArc::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::RemoveNodeDelegate),
		EventDataRemoveNode::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::HighlightNodeDelegate),
		EventDataHighlightNode::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::EditMapNodeDelegate),
		EventDataEditMapNode::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowMapNodeDelegate),
		EventDataShowMapNode::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ClearMapDelegate),
		EventDataClear::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::EditPathingGraphDelegate),
		EventDataEditPathing::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowPathingGraphDelegate),
		EventDataShowPathing::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathingMapDelegate),
		EventDataCreatePathingMap::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathingNodeDelegate),
		EventDataCreatePathingNode::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathingGraphDelegate),
		EventDataCreatePathing::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::SimulateExploringDelegate),
		EventDataSimulateExploring::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::SimulatePathingDelegate),
		EventDataSimulatePathing::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowNodeVisibilityDelegate),
		EventDataNodeVisibility::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowNodeConnectionDelegate),
		EventDataNodeConnection::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowArcConnectionDelegate),
		EventDataArcConnection::skEventType);

	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::SaveMapDelegate),
		EventDataSaveMap::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::EditMapDelegate),
		EventDataEditMap::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::ShowMapDelegate),
		EventDataShowMap::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::CreateMapDelegate),
		EventDataCreateMap::skEventType);
	pGlobalEventManager->RemoveListener(
		MakeDelegate(this, &QuakeAIEditorView::CreatePathDelegate),
		EventDataCreatePath::skEventType);
}

void QuakeAIEditorView::OpenConsole(float scale, const wchar_t* line)
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

void QuakeAIEditorView::ToggleFreeMove()
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

void QuakeAIEditorView::ToggleFreeMoveAlt()
{
	if (mSettings.doubletapJump)
		ToggleFreeMove();
}

void QuakeAIEditorView::TogglePitchMove()
{
	bool pitchMove = !Settings::Get()->GetBool("pitch_move");
	Settings::Get()->Set("pitch_move", pitchMove ? "true" : "false");

	if (pitchMove)
		mUI->ShowTranslatedStatusText("Pitch move mode enabled");
	else
		mUI->ShowTranslatedStatusText("Pitch move mode disabled");
}

void QuakeAIEditorView::ToggleFast()
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

void QuakeAIEditorView::ToggleNoClip()
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

void QuakeAIEditorView::ToggleCinematic()
{
	bool cinematic = !Settings::Get()->GetBool("cinematic");
	Settings::Get()->Set("cinematic", cinematic ? "true" : "false");

	if (cinematic)
		mUI->ShowTranslatedStatusText("Cinematic mode enabled");
	else
		mUI->ShowTranslatedStatusText("Cinematic mode disabled");
}

// Autoforward by toggling continuous forward.
void QuakeAIEditorView::ToggleAutoforward()
{
	bool autorunEnabled = !Settings::Get()->GetBool("continuous_forward");
	Settings::Get()->Set("continuous_forward", autorunEnabled ? "true" : "false");

	if (autorunEnabled)
		mUI->ShowTranslatedStatusText("Automatic forward enabled");
	else
		mUI->ShowTranslatedStatusText("Automatic forward disabled");
}

void QuakeAIEditorView::ToggleFog()
{
	bool fogEnabled = Settings::Get()->GetBool("enable_fog");
	Settings::Get()->SetBool("enable_fog", !fogEnabled);
	if (fogEnabled)
		mUI->ShowTranslatedStatusText("Fog disabled");
	else
		mUI->ShowTranslatedStatusText("Fog enabled");
}

void QuakeAIEditorView::ToggleDebug()
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

void QuakeAIEditorView::IncreaseViewRange()
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

void QuakeAIEditorView::DecreaseViewRange()
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

void QuakeAIEditorView::ToggleFullViewRange()
{
	mUI->ShowTranslatedStatusText("Disabled unlimited viewing range");
}

void QuakeAIEditorView::CheckZoomEnabled()
{

}


/****************************************************************************/
/****************************************************************************
 Shutdown / cleanup
 ****************************************************************************/
 /****************************************************************************/

void QuakeAIEditorView::ExtendedResourceCleanup()
{
	// Extended resource accounting
	LogInformation("Game resources after cleanup:");
}

void QuakeAIEditorView::Shutdown()
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

void QuakeAIEditorView::ShowPathingMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter)
{
	std::string form = 
		"form_version[3]size[10,10]position[0.2,0.35]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
        "image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
        "image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
        "image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
        "tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
        "container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph;#4bdd42,Pathing Graph,,,,,";

	const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		if (clusters.find(node->GetCluster()) != clusters.end())
		{
			std::string str = std::to_string(node->GetId());
			std::string::size_type start = str.find(filter, 0);
			if (start != str.npos)
			{
				Vector3<float> pos = node->GetPosition();
				form += ",,,#ffffff," +
					std::to_string(node->GetId()) + "," +
					std::to_string(node->GetCluster()) + "," +
					std::to_string(node->GetActorId()) + "," +
					std::to_string(int(round(pos[0]))) + " " +
					std::to_string(int(round(pos[1]))) + " " +
					std::to_string(int(round(pos[2])));
			}
		}
	}
	form += "]"
		"field[0.25,7.5;7,0.75;te_orientation;Orientation;0]"
		"button[0.25,8.5;2.25,0.75;btn_respawn; Respawn]"
		"button[2.5,8.5;2.25,0.75;btn_pathing; Pathing]"
		"button[4.75,8.5;2.25,0.75;btn_exploring; Exploring]"
		"button[7,8.5;2.25,0.75;btn_reset; Reset]";

	/* Create menu */
	/* Note: FormSource and PathingFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<PathingFormHandler> textDst = std::make_shared<PathingFormHandler>("PATHING");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "PATHING")
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("PATHING");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::ShowMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter)
{
	std::string form =
		"form_version[3]size[10,14]position[0.2,0.5]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
		"image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
		"tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
		"container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph_nodes;#4bdd42,Map Nodes,,,,,";
	const PathingNodeMap& pathingNodes = mMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		if (clusters.find(node->GetCluster()) != clusters.end())
		{
			std::string str = std::to_string(node->GetId());
			std::string::size_type start = str.find(filter, 0);
			if (start != str.npos)
			{
				Vector3<float> pos = node->GetPosition();
				form += ",,,#ffffff," +
					std::to_string(node->GetId()) + "," +
					std::to_string(node->GetCluster()) + "," +
					std::to_string(node->GetActorId()) + "," +
					std::to_string(int(round(pos[0]))) + " " +
					std::to_string(int(round(pos[1]))) + " " +
					std::to_string(int(round(pos[2])));
			}
		}
	}
	form += "]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=4.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,7;9.25,5.75;graph_arcs;#4bdd42,Map Arcs,,,,,";
	/*
	for (auto node : mMap->GetNodes())
	{
		if (clusters.find(node->GetCluster()) != clusters.end())
		{
			for (auto const& arc : node->GetArcs())
			{
				form += ",,,#ffffff," +
					std::to_string(arc->GetId()) + "," +
					std::to_string(arc->GetNode()->GetId()) + "," +
					std::to_string(arc->GetType()) + "," +
					std::to_string(arc->GetWeight());
			}
		}
	}
	*/
	form += "]"
		"button[0.5,13;2.5,0.75;btn_visibility; Visibility]"
		"button[3,13;2.5,0.75;btn_connection; Connection]"
		"button[5.5,13;2.5,0.75;btn_save_all; Save All]"
		"button[8,13;1.5,0.75;btn_reset; Reset]";

	/* Create menu */
	/* Note: FormSource and MapFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<MapFormHandler> textDst = std::make_shared<MapFormHandler>("MAP");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "MAP")
	{
		std::shared_ptr<UIForm>& formUI = std::static_pointer_cast<UIForm>(mUI->GetForm());
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);

		std::shared_ptr<BaseUITable>& table = std::static_pointer_cast<BaseUITable>(
			formUI->GetElementFromId(formUI->GetField("graph_nodes")));
		table->SetSelected(-1);
		table = std::static_pointer_cast<BaseUITable>(
			formUI->GetElementFromId(formUI->GetField("graph_arcs")));
		table->SetSelected(-1);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("MAP");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::ShowMap(PathingNode* pNode)
{
	std::string form =
		"form_version[3]size[10,14]position[0.2,0.5]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
		"image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
		"tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
		"container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph_nodes;#4bdd42,Map Nodes,,,,,";
	unsigned int selectedIdx = 2, nodeIdx = 2;
	const PathingNodeMap& pathingNodes = mMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		if (node->GetCluster() == pNode->GetCluster())
		{
			Vector3<float> pos = node->GetPosition();
			form += ",,,#ffffff," +
				std::to_string(node->GetId()) + "," +
				std::to_string(node->GetCluster()) + "," +
				std::to_string(node->GetActorId()) + "," +
				std::to_string(int(round(pos[0]))) + " " +
				std::to_string(int(round(pos[1]))) + " " +
				std::to_string(int(round(pos[2])));

			if (node == pNode)
				selectedIdx = nodeIdx;
			nodeIdx++;
		}
	}
	form += ";" + std::to_string(selectedIdx) + "]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=4.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,7;9.25,5.75;graph_arcs;#4bdd42,Map Arcs,,,,,";
	for (auto const& arc : pNode->GetArcs())
	{
		PathingArc* pArc = arc.second;

		form += ",,,#ffffff," +
			std::to_string(pArc->GetId()) + "," +
			std::to_string(pArc->GetNode()->GetId()) + "," +
			std::to_string(pArc->GetType()) + "," +
			std::to_string(pArc->GetWeight());
	}
	form += "]"
		"button[0.5,13;2.5,0.75;btn_visibility; Visibility]"
		"button[3,13;2.5,0.75;btn_connection; Connection]"
		"button[5.5,13;2.5,0.75;btn_save_all; Save All]"
		"button[8,13;1.5,0.75;btn_reset; Reset]";

	/* Create menu */
	/* Note: FormSource and MapFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<MapFormHandler> textDst = std::make_shared<MapFormHandler>("MAP");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "MAP")
	{
		std::shared_ptr<UIForm>& formUI = std::static_pointer_cast<UIForm>(mUI->GetForm());
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		//formUI->SetTextDestination(textDst);

		std::shared_ptr<BaseUITable>& table = std::static_pointer_cast<BaseUITable>(
			formUI->GetElementFromId(formUI->GetField("graph_arcs")));
		table->SetSelected(-1);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("MAP");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::CreatePathingMap(const std::string& filter)
{
	std::string form =
		"form_version[3]size[10,10]position[0.2,0.35]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
		"image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
		"tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
		"container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph;#4bdd42,Pathing Graph,,,,,";

	const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		std::string str = std::to_string(node->GetId());
		std::string::size_type start = str.find(filter, 0);
		if (start != str.npos)
		{
			Vector3<float> pos = node->GetPosition();
			form += ",,,#ffffff," +
				std::to_string(node->GetId()) + "," +
				std::to_string(node->GetCluster()) + "," +
				std::to_string(node->GetActorId()) + "," +
				std::to_string(int(round(pos[0]))) + " " +
				std::to_string(int(round(pos[1]))) + " " +
				std::to_string(int(round(pos[2])));
		}
	}
	form += "]"
		"field[0.25,7.5;7,0.75;te_orientation;Orientation;0]"
		"button[0.5,8.5;2.5,0.75;btn_respawn; Respawn]"
		"button[3,8.5;2.5,0.75;btn_create_node; Add Node]"
		"button[5.5,8.5;2,0.75;btn_pathing; Pathing]"
		"button[7.5,8.5;1.5,0.75;btn_save; Save]";

	/* Create menu */
	/* Note: FormSource and CreatePathingMapFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<CreatePathingMapFormHandler> textDst = std::make_shared<CreatePathingMapFormHandler>("CREATE_PATHING");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "CREATE_PATHING")
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("CREATE_PATHING");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::EditPathingMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter)
{
	std::string form =
		"form_version[3]size[10,10]position[0.2,0.35]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
		"image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
		"tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
		"container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph;#4bdd42,Pathing Graph,,,,,";

	const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		if (clusters.find(node->GetCluster()) != clusters.end())
		{
			std::string str = std::to_string(node->GetId());
			std::string::size_type start = str.find(filter, 0);
			if (start != str.npos)
			{
				Vector3<float> pos = node->GetPosition();
				form += ",,,#ffffff," +
					std::to_string(node->GetId()) + "," +
					std::to_string(node->GetCluster()) + "," +
					std::to_string(node->GetActorId()) + "," +
					std::to_string(int(round(pos[0]))) + " " +
					std::to_string(int(round(pos[1]))) + " " +
					std::to_string(int(round(pos[2])));
			}
		}
	}
	form += "]"
		"field[0.25,7.5;7,0.75;te_orientation;Orientation;0]"
		"button[0.25,8.5;2,0.75;btn_respawn; Respawn]"
		"button[2.25,8.5;2,0.75;btn_create_node; Node]"
		"button[4.25,8.5;2,0.75;btn_pathing; Pathing]"
		"button[6.25,8.5;1.5,0.75;btn_save; Save]"
		"button[7.75,8.5;1.5,0.75;btn_reset; Reset]";

	/* Create menu */
	/* Note: FormSource and EditPathingFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<EditPathingFormHandler> textDst = std::make_shared<EditPathingFormHandler>("EDIT_PATHING");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "EDIT_PATHING")
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->GetForm();
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("EDIT_PATHING");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::EditMap(const std::map<unsigned short, unsigned short>& clusters, const std::string& filter)
{
	std::string form =
		"form_version[3]size[10,14]position[0.2,0.5]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
		"image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
		"tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
		"container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph_nodes;#4bdd42,Map Nodes,,,,,";

	const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		if (clusters.find(node->GetCluster()) != clusters.end())
		{
			std::string str = std::to_string(node->GetId());
			std::string::size_type start = str.find(filter, 0);
			if (start != str.npos)
			{
				Vector3<float> pos = node->GetPosition();
				form += ",,,#ffffff," +
					std::to_string(node->GetId()) + "," +
					std::to_string(node->GetCluster()) + "," +
					std::to_string(node->GetActorId()) + "," +
					std::to_string(int(round(pos[0]))) + " " +
					std::to_string(int(round(pos[1]))) + " " +
					std::to_string(int(round(pos[2])));
			}
		}
	}
	form += "]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,7;9.25,5.75;graph_arcs;#4bdd42,Map Arcs,,,,,";
	/*
	for (auto node : mPathingMap->GetNodes())
	{
		if (clusters.find(node->GetCluster()) != clusters.end())
		{
			for (auto const& arc : node->GetArcs())
			{
				form += ",,,#ffffff," +
					std::to_string(arc->GetId()) + "," +
					std::to_string(arc->GetNode()->GetId()) + "," +
					std::to_string(arc->GetType()) + "," +
					std::to_string(arc->GetWeight());
			}
		}
	}
	*/
	form += "]"
		"button[1,13;1.5,0.75;btn_clear; Clear]"
		"button[2.5,13;2.5,0.75;btn_connection; Connection]"
		"button[5,13;2.5,0.75;btn_remove; Remove]"
		"button[7.5,13;1.5,0.75;btn_save; Save]";

	/* Create menu */
	/* Note: FormSource and MapFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<EditMapFormHandler> textDst = std::make_shared<EditMapFormHandler>("EDIT_MAP");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "EDIT_MAP")
	{
		std::shared_ptr<UIForm>& formUI = std::static_pointer_cast<UIForm>(mUI->GetForm());
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		formUI->SetTextDestination(textDst);

		std::shared_ptr<BaseUITable>& table = std::static_pointer_cast<BaseUITable>(
			formUI->GetElementFromId(formUI->GetField("graph_nodes")));
		table->SetSelected(-1);
		table = std::static_pointer_cast<BaseUITable>(
			formUI->GetElementFromId(formUI->GetField("graph_arcs")));
		table->SetSelected(-1);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("EDIT_MAP");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::EditMap(PathingNode* pNode)
{
	std::string form =
		"form_version[3]size[10,14]position[0.2,0.5]"
		"field[0.25,0.25;7,0.75;te_search;;]field_close_on_enter[te_search;false]container[7.25,0.25]"
		"image_button[0,0;0.75,0.75;art/quake/textures/search.png;btn_mp_search;]"
		"image_button[0.75,0;0.75,0.75;art/quake/textures/clear.png;btn_mp_clear;]"
		"image_button[1.5,0;0.75,0.75;art/quake/textures/refresh.png;btn_mp_refresh;]"
		"tooltip[btn_mp_clear;Clear]tooltip[btn_mp_search;Search]tooltip[btn_mp_refresh;Refresh]"
		"container_end[]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=9.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,1;9.25,5.75;graph_nodes;#4bdd42,Map Nodes,,,,,";

	unsigned int selectedIdx = 2, nodeIdx = 2;
	const PathingNodeMap& pathingNodes = mPathingMap->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;
		if (node->GetCluster() == pNode->GetCluster())
		{
			Vector3<float> pos = node->GetPosition();
			form += ",,,#ffffff," +
				std::to_string(node->GetId()) + "," +
				std::to_string(node->GetCluster()) + "," +
				std::to_string(node->GetActorId()) + "," +
				std::to_string(int(round(pos[0]))) + " " +
				std::to_string(int(round(pos[1]))) + " " +
				std::to_string(int(round(pos[2])));

			if (node == pNode)
				selectedIdx = nodeIdx;
			nodeIdx++;
		}
	}
	form += ";" + std::to_string(selectedIdx) + "]"
		"tablecolumns[color,span=1;text,align=inline;"
		"color,span=4;text,align=inline,width=4.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25;"
		"text,align=inline,width=3.25]"
		"tableoptions[background=#00000000;border=false]"
		"table[0.25,7;9.25,5.75;graph_arcs;#4bdd42,Map Arcs,,,,,";
	for (auto const& arc : pNode->GetArcs())
	{
		PathingArc* pArc = arc.second;

		form += ",,,#ffffff," +
			std::to_string(pArc->GetId()) + "," +
			std::to_string(pArc->GetNode()->GetId()) + "," +
			std::to_string(pArc->GetType()) + "," +
			std::to_string(pArc->GetWeight());
	}
	form += "]"
		"button[0.5,13;1.5,0.75;btn_clear; Clear]"
		"button[2,13;2.5,0.75;btn_connection; Connection]"
		"button[4.5,13;2,0.75;btn_remove; Remove]"
		"button[6.5,13;3,0.75;btn_remove_type; RemoveType]";

	/* Create menu */
	/* Note: FormSource and MapFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(form);
	std::shared_ptr<EditMapFormHandler> textDst = std::make_shared<EditMapFormHandler>("EDIT_MAP");

	RectangleShape<2, int> rectangle;
	rectangle.mCenter = Vector2<int>{ 50, 50 };
	rectangle.mExtent = Vector2<int>{ 100, 100 };

	if (mUI->mFormName == "EDIT_MAP")
	{
		std::shared_ptr<UIForm>& formUI = std::static_pointer_cast<UIForm>(mUI->GetForm());
		formUI->SetFormPrepend(formPr);
		formUI->SetFormSource(formSrc);
		//formUI->SetTextDestination(textDst);

		std::shared_ptr<BaseUITable>& table = std::static_pointer_cast<BaseUITable>(
			formUI->GetElementFromId(formUI->GetField("graph_arcs")));
		table->SetSelected(-1);
	}
	else
	{
		std::shared_ptr<BaseUIForm>& formUI = mUI->UpdateForm("EDIT_MAP");
		formUI.reset(new UIForm(mUI.get(), -1, rectangle, formSrc, textDst, formPr, false));
		formUI->SetParent(mUI->GetRootUIElement());
		formUI->OnInit();
	}
}

void QuakeAIEditorView::ShowPauseMenu()
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
		<< "button_exit[4," << (yPos++) << ";3,0.5;btn_edit_map;"
		<< "Edit Map" << "]" << "field[4.95,0;5,1.5;;" << "Main Menu" << ";]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_create_path;"
		<< "Create Path" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_edit_path;"
		<< "Edit Path" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_create_map;"
		<< "Create Map" << "]";

	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_show_map;"
		<< "Show Map" << "]"
	/*
	os << "button_exit[4," << (yPos++) << ";3,0.5;btn_exit_menu;"
		<< "Exit Editor" << "]"
	*/
		<< "textarea[7.5,0.25;3.9,6.25;;" << controlText << ";]"
		<< "textarea[0.4,0.25;3.9,6.25;;" << "Quake \n"
		<< "\n" << "Editor info:" << "\n";
	os << ";]";

	/* Create menu */
	/* Note: FormSource and EditorFormHandler are deleted by FormMenu */
	std::string formPr;
	std::shared_ptr<FormSource> formSrc = std::make_shared<FormSource>(os.str());
	std::shared_ptr<EditorFormHandler> textDst = std::make_shared<EditorFormHandler>("PAUSE_MENU");

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

	formUI->SetFocus("btn_edit");
}

void  QuakeAIEditorView::UpdateControllers(unsigned int timeMs, unsigned long deltaMs)
{
	if (mCameraController)
		mCameraController->OnUpdate(timeMs, deltaMs);

	if (mPlayerController)
		mPlayerController->OnUpdate(timeMs, deltaMs);
}

void QuakeAIEditorView::UpdateSound(float dTime)
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

void QuakeAIEditorView::ProcessPlayerInteraction(float dTime, bool showHud, bool showDebug)
{
	ClearWasKeyPressed();
	ClearWasKeyReleased();
}

bool QuakeAIEditorView::InitSound()
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

	mSoundMaker = std::make_shared<EditorSoundMaker>(mSoundMgr.get());
	if (!mSoundMaker)
	{
		LogError("Failed to Initialize OpenAL audio");
		return false;
	}

	//mSoundMaker->RegisterReceiver(mEventMgr);

	return true;
}