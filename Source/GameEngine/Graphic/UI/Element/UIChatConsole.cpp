/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "UIChatConsole.h"
#include "UIFont.h"

#include "Core/OS/Os.h"
#include "Core/Logger/Logger.h"

#include "Core/Event/EventManager.h"
#include "Core/Event/Event.h"

#include "Application/Settings.h"
#include "Application/System/KeyEvent.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"


UIChatConsole::UIChatConsole(BaseUI* ui, int id, RectangleShape<2, int> rectangle) :
    BaseUIElement(UIET_CHAT_CONSOLE, id, rectangle), mUI(ui), mAnimateTimeOld(Timer::GetTime())
{
	mBlendState = std::make_shared<BlendState>();
	mBlendState->mTarget[0].enable = true;
	mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
	mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
	mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    // basic color effect
    {
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

        std::shared_ptr<VisualEffect> effect =
            std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
        mVisualBackground = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

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

        mEffect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

        // Create the geometric object for drawing.
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
    }

	// load background settings
	int consoleAlpha = Settings::Get()->GetInt("console_alpha");
	mBackgroundColor.SetAlpha(std::min(std::max(consoleAlpha, 0), 255));

	// load the background texture depending on settings
	resHandle = ResCache::Get()->GetHandle(&BaseResource(L"background_chat.jpg"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		mBackground = extra->GetImage();
		mBackgroundColor.SetRed(255);
		mBackgroundColor.SetGreen(255);
		mBackgroundColor.SetBlue(255);
	}
	else
	{
		Vector3<float> consoleColor = Settings::Get()->GetVector3("console_color");
		mBackgroundColor.SetRed(std::min(std::max((int)round(consoleColor[0]), 0), 255));
		mBackgroundColor.SetGreen(std::min(std::max((int)round(consoleColor[1]), 0), 255));
		mBackgroundColor.SetBlue(std::min(std::max((int)round(consoleColor[2]), 0), 255));
	}

	std::shared_ptr<BaseUIFont> textFont = mUI->GetBuiltInFont();
	Vector2<int> dim = textFont->GetDimension(L"M");
	mFontSize = {(unsigned int)dim[0], (unsigned int)dim[1]};
	mFontSize[0] = std::max(mFontSize[0], (unsigned int)1);
	mFontSize[1] = std::max(mFontSize[1], (unsigned int)1);

	mFont = mUI->GetSkin()->GetFont();

	// set default cursor options
	SetCursor(true, true, 2.f, 0.1f);
}

UIChatConsole::~UIChatConsole()
{
}

void UIChatConsole::OpenConsole(float scale)
{
	LogAssert(scale > 0.0f && scale <= 1.0f, "invalid scale");

	mOpen = true;
	mDesiredHeightFraction = scale;
	mDesiredHeight = scale * mScreenSize[1];
	ReformatConsole();
	mAnimateTimeOld = Timer::GetTime();

	BaseUIElement::SetVisible(true);
	mUI->SetFocus(shared_from_this());
}

bool UIChatConsole::IsOpen() const
{
	return mOpen;
}

bool UIChatConsole::IsOpenInhibited() const
{
	return mOpenInhibited > 0;
}

void UIChatConsole::CloseConsole()
{
	mOpen = false;
	mUI->RemoveFocus(shared_from_this());
}

void UIChatConsole::CloseConsoleAtOnce()
{
	CloseConsole();
	mHeight = 0;
	RecalculateConsolePosition();
}

void UIChatConsole::ReplaceAndAddToHistory(const std::wstring& line)
{
	ChatPrompt& prompt = mChatBackend->GetPrompt();
	prompt.AddToHistory(prompt.GetLine());
	prompt.Replace(line);
}

void UIChatConsole::SetCursor(bool visible, bool blinking, float blinkSpeed, float relativeHeight)
{
	if (visible)
	{
		if (blinking)
		{
			// leave mCursorBlink unchanged
			mCursorBlinkSpeed = blinkSpeed;
		}
		else
		{
			mCursorBlink = 0x8000;  // on
			mCursorBlinkSpeed = 0.0;
		}
	}
	else
	{
		mCursorBlink = 0;  // off
		mCursorBlinkSpeed = 0.0;
	}
	mCursorHeight = relativeHeight;
}

void UIChatConsole::Draw()
{
	if(!IsVisible())
		return;

	// Check screen size
	Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
	if (screenSize != mScreenSize)
	{
		// screen size has changed
		// scale current console height to new window size
		if (mScreenSize[1] != 0)
			mHeight = mHeight * screenSize[1] / mScreenSize[1];
		mScreenSize = screenSize;
		mDesiredHeight = mDesiredHeightFraction * mScreenSize[1];
		ReformatConsole();
	}

	// Animation
	unsigned int now = Timer::GetTime();
	Animate(now - mAnimateTimeOld);
	mAnimateTimeOld = now;

	// Draw console elements if visible
	if (mHeight > 0)
	{
		DrawBackground();
		DrawChatText();
		DrawPrompt();
	}

	BaseUIElement::Draw();
}

void UIChatConsole::ReformatConsole()
{
	int cols = (int)(mScreenSize[0] / mFontSize[0]) - 2; // make room for a margin (looks better)
	int rows = (int)(mDesiredHeight / mFontSize[1]) - 1; // make room for the input prompt
	if (cols <= 0 || rows <= 0)
		cols = rows = 0;
	RecalculateConsolePosition();
	mChatBackend->Reformat(cols, rows);
}

void UIChatConsole::RecalculateConsolePosition()
{
    RectangleShape<2, int> rect;
    rect.mExtent = Vector2<int>{ (int)mScreenSize[0], mHeight };
    rect.mCenter = rect.mExtent / 2;

	mDesiredRect = rect;
	RecalculateAbsolutePosition(false);
}

void UIChatConsole::Animate(unsigned int deltaMs)
{
	// animate the console height
	int goal = mOpen ? (int)mDesiredHeight : 0;

	// Set invisible if close animation finished (reset by OpenConsole)
	// This function (animate()) is never called once its visibility becomes false so do not
	//		actually set visible to false before the inhibited period is over
	if (!mOpen && mHeight == 0 && mOpenInhibited == 0)
		BaseUIElement::SetVisible(false);

	if (mHeight != goal)
	{
		int maxChange = (int)(deltaMs * mScreenSize[1] * (mHeightSpeed / 1000.f));
		if (maxChange == 0)
            maxChange = 1;

		if (mHeight < goal)
		{
			// increase height
			if (mHeight + maxChange < goal)
				mHeight += maxChange;
			else
				mHeight = goal;
		}
		else
		{
			// decrease height
			if (mHeight > goal + maxChange)
				mHeight -= maxChange;
			else
				mHeight = goal;
		}

		RecalculateConsolePosition();
	}

	// blink the cursor
	if (mCursorBlinkSpeed != 0.0)
	{
		unsigned int blinkIncrease = (unsigned int)(0x10000 * deltaMs * (mCursorBlinkSpeed / 1000.f));
		if (blinkIncrease == 0)
            blinkIncrease = 1;
		mCursorBlink = ((mCursorBlink + blinkIncrease) & 0xffff);
	}

	// decrease open inhibit counter
	if (mOpenInhibited > deltaMs)
		mOpenInhibited -= deltaMs;
	else
		mOpenInhibited = 0;
}

void UIChatConsole::DrawBackground()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	Renderer::Get()->SetBlendState(mBlendState);

	if (mBackground != NULL)
	{
        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent[0] = mBackground->GetDimension(0);
        tcoordRect.mExtent[1] = mBackground->GetDimension(1);
        tcoordRect.mCenter = tcoordRect.mExtent / 2;
        RectangleShape<2, int> sourceRect;
        sourceRect.mExtent = Vector2<int>{ (int)mScreenSize[0], mHeight };
        sourceRect.mCenter[0] = sourceRect.mExtent[0] / 2;
        sourceRect.mCenter[1] = -sourceRect.mExtent[1] / 2;
        SColor imageColors[] = { 
            mBackgroundColor, mBackgroundColor, mBackgroundColor, mBackgroundColor };

        skin->Draw2DTexture(mVisual, sourceRect, tcoordRect, imageColors);
	}
	else
	{
        RectangleShape<2, int> sourceRect;
        sourceRect.mExtent = Vector2<int>{ (int)mScreenSize[0], mHeight };
        sourceRect.mCenter = sourceRect.mExtent / 2;

        skin->Draw2DRectangle(mBackgroundColor, mVisualBackground, sourceRect, &mAbsoluteClippingRect);
	}

	Renderer::Get()->SetDefaultBlendState();
}

void UIChatConsole::DrawChatText()
{
	if (mFont == NULL)
		return;

	ChatBuffer& buf = mChatBackend->GetConsoleBuffer();
	for (unsigned int row = 0; row < buf.GetRows(); ++row)
	{
		const ChatFormattedLine& line = buf.GetFormattedLine(row);
		if (line.fragments.empty())
			continue;

		int x = mFontSize[0];
		int y = (int)(row * mFontSize[1] + mHeight - mDesiredHeight);
		if (y + mFontSize[1] < 0)
			continue;

		RectangleShape<2, int> destRect;
		destRect.mExtent = Vector2<int>{(int)mFontSize[0], (int)mFontSize[1] };
		destRect.mCenter = destRect.mExtent / 2 + Vector2<int>{x, y};

		std::wstring text;
		for (const ChatFormattedFragment& fragment : line.fragments)
			text += fragment.text.C_Str();

		mFont->Draw(text, destRect,
			SColor(255, 255, 255, 255), false, false, &mAbsoluteClippingRect);
	}
}

void UIChatConsole::DrawPrompt()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

	if (!mFont)
		return;

	unsigned int row = mChatBackend->GetConsoleBuffer().GetRows();
	int x = mFontSize[0];
	int y = (int)(row * mFontSize[1] + mHeight - mDesiredHeight);

	ChatPrompt& prompt = mChatBackend->GetPrompt();
	std::wstring promptText = prompt.GetVisiblePortion();

    RectangleShape<2, int> destRect;
    destRect.mExtent = Vector2<int>{(int)mFontSize[0], (int)mFontSize[1]};
    destRect.mCenter = destRect.mExtent / 2 + Vector2<int>{x, y};

	// Draw the cursor during on periods
	if ((mCursorBlink & 0x8000) != 0)
		promptText += L"_";

	mFont->Draw(promptText, destRect,
		SColor(255, 255, 255, 255), false, false, &mAbsoluteClippingRect);
}

bool UIChatConsole::OnEvent(const Event& evt)
{
	ChatPrompt& prompt = mChatBackend->GetPrompt();

	if(evt.mEventType == ET_KEY_INPUT_EVENT && evt.mKeyInput.mPressedDown)
	{
		// Key input
		if (KeyAction(evt.mKeyInput) == GetKeySetting("keymap_console"))
        {
			CloseConsole();

			// inhibit open so the_game doesn't reopen immediately
			mOpenInhibited = 50;
			mCloseOnEnter = false;
			return true;
		}

		if (evt.mKeyInput.mKey == KEY_ESCAPE)
        {
			CloseConsoleAtOnce();
			mCloseOnEnter = false;
			// inhibit open so the_game doesn't reopen immediately
			mOpenInhibited = 1; // so the ESCAPE button doesn't open the "pause menu"
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_PRIOR)
		{
			mChatBackend->ScrollPageUp();
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_NEXT)
		{
			mChatBackend->ScrollPageDown();
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_RETURN)
		{
			prompt.AddToHistory(prompt.GetLine());
			std::wstring text = prompt.Replace(L"");

			std::shared_ptr<EventDataChatMessage>
				pSendChatMessageEvent(new EventDataChatMessage(text));
			BaseEventManager::Get()->QueueEvent(pSendChatMessageEvent);
			if (mCloseOnEnter) 
            {
				CloseConsoleAtOnce();
				mCloseOnEnter = false;
			}
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_UP)
		{
			// Up pressed
			// Move back in history
			prompt.HistoryPrevious();
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_DOWN)
		{
			// Down pressed
			// Move forward in history
			prompt.HistoryNext();
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_LEFT || evt.mKeyInput.mKey == KEY_RIGHT)
		{
			// Left/right pressed
			// Move/select character/word to the left depending on control and shift keys
			ChatPrompt::CursorOp op = evt.mKeyInput.mShift ?
				ChatPrompt::CURSOROP_SELECT : ChatPrompt::CURSOROP_MOVE;
			ChatPrompt::CursorOpDir dir = evt.mKeyInput.mKey == KEY_LEFT ?
				ChatPrompt::CURSOROP_DIR_LEFT : ChatPrompt::CURSOROP_DIR_RIGHT;
			ChatPrompt::CursorOpScope scope = evt.mKeyInput.mControl ?
				ChatPrompt::CURSOROP_SCOPE_WORD : ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			prompt.CursorOperation(op, dir, scope);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_HOME)
		{
			// Home pressed
			// move to beginning of line
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_END)
		{
			// End pressed
			// move to end of line
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_MOVE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_BACK)
		{
			// Backspace or Ctrl-Backspace pressed
			// delete character / word to the left
			ChatPrompt::CursorOpScope scope = evt.mKeyInput.mControl ?
				ChatPrompt::CURSOROP_SCOPE_WORD : ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				scope);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_DELETE)
		{
			// Delete or Ctrl-Delete pressed
			// delete character / word to the right
			ChatPrompt::CursorOpScope scope = evt.mKeyInput.mControl ?
				ChatPrompt::CURSOROP_SCOPE_WORD : ChatPrompt::CURSOROP_SCOPE_CHARACTER;
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				scope);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_KEY_A && evt.mKeyInput.mControl)
		{
			// Ctrl-A pressed
			// Select all text
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_SELECT,
				ChatPrompt::CURSOROP_DIR_LEFT, // Ignored
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_KEY_C && evt.mKeyInput.mControl)
		{
			// Ctrl-C pressed
			// Copy text to clipboard
			if (prompt.GetCursorLength() <= 0)
				return true;
			std::wstring wselected = prompt.GetSelection();
			std::string selected = ToString(wselected);
            //CopyToClipboard(selected.c_str());
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_KEY_V && evt.mKeyInput.mControl)
		{
			// Ctrl-V pressed
			// paste text from clipboard
			if (prompt.GetCursorLength() > 0) 
            {
				// Delete selected section of text
				prompt.CursorOperation(
					ChatPrompt::CURSOROP_DELETE,
					ChatPrompt::CURSOROP_DIR_LEFT, // Ignored
					ChatPrompt::CURSOROP_SCOPE_SELECTION);
			}
			/*
            const char* text;// = GetTextFromClipboard();
			if (!text)
				return true;
			std::basic_string<unsigned char> str((const unsigned char*)text);
			prompt.Input(std::wstring(str.begin(), str.end()));
			*/
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_KEY_X && evt.mKeyInput.mControl)
		{
			// Ctrl-X pressed
			// Cut text to clipboard
			if (prompt.GetCursorLength() <= 0)
				return true;
			std::wstring wselected = prompt.GetSelection();
			std::string selected = ToString(wselected);
            //CopyToClipboard(selected.c_str());
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT, // Ignored
				ChatPrompt::CURSOROP_SCOPE_SELECTION);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_KEY_U && evt.mKeyInput.mControl)
		{
			// Ctrl-U pressed
			// kill line to left end
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_LEFT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_KEY_K && evt.mKeyInput.mControl)
		{
			// Ctrl-K pressed
			// kill line to right end
			prompt.CursorOperation(
				ChatPrompt::CURSOROP_DELETE,
				ChatPrompt::CURSOROP_DIR_RIGHT,
				ChatPrompt::CURSOROP_SCOPE_LINE);
			return true;
		}
		else if(evt.mKeyInput.mKey == KEY_TAB)
		{
			// Tab or Shift-Tab pressed
			// Nick completion
            std::list<std::string> names;// = m_client->getConnectedPlayerNames();
			bool backwards = evt.mKeyInput.mShift;
			prompt.NickCompletion(names, backwards);
			return true;
		} 
        else if (!iswcntrl(evt.mKeyInput.mChar) && !evt.mKeyInput.mControl)
        {
			prompt.Input(evt.mKeyInput.mChar);
			return true;
		}
	}
	else if(evt.mEventType == ET_MOUSE_INPUT_EVENT)
	{
		if(evt.mMouseInput.mEvent == MIE_MOUSE_WHEEL)
		{
			int rows = (int)round(-3.f * evt.mMouseInput.mWheel);
			mChatBackend->Scroll(rows);
		}
	}

	return mParent ? mParent->OnEvent(evt) : false;
}

void UIChatConsole::SetVisible(bool visible)
{
	mOpen = visible;
	BaseUIElement::SetVisible(visible);
	if (!visible) 
    {
		mHeight = 0;
		RecalculateConsolePosition();
	}
}

