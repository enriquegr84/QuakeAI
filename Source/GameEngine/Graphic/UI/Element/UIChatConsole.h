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

#ifndef UICHATCONSOLE_H
#define UICHATCONSOLE_H

#include "GameEngineStd.h"

#include "UIElement.h"

#include "Core/Utility/Chat.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUIFont;

class UIChatConsole : public BaseUIElement
{
public:

	UIChatConsole(BaseUI* ui, int id, RectangleShape<2, int> rectangle);
	virtual ~UIChatConsole();

	// Open the console (height = desired fraction of screen size)
	// This doesn't open immediately but initiates an animation.
	// You should call IsOpenInhibited() before this.
	void OpenConsole(float scale);

	bool IsOpen() const;

    void SetChat(ChatBackend* backend) { mChatBackend = backend; }

	// Check if the console should not be opened at the moment
	// This is to avoid reopening the console immediately after closing
	bool IsOpenInhibited() const;
	// Close the console, equivalent to OpenConsole(0).
	// This doesn't close immediately but initiates an animation.
	void CloseConsole();
	// Close the console immediately, without animation.
	void CloseConsoleAtOnce();
	// Set whether to close the console after the user presses enter.
	void SetCloseOnEnter(bool close) { mCloseOnEnter = close; }

	// Replace actual line when adding the actual to the history (if there is any)
	void ReplaceAndAddToHistory(const std::wstring& line);

	// Change how the cursor looks
	void SetCursor(bool visible, bool blinking = false,
		float blinkSpeed = 1.0, float relativeHeight = 1.0);

	// Irrlicht draw method
	virtual void Draw();

	virtual bool OnEvent(const Event& evt);

	virtual void SetVisible(bool visible);

	virtual bool AcceptsIME() { return true; }

private:
	void ReformatConsole();
	void RecalculateConsolePosition();

	// These methods are called by draw
	void Animate(unsigned int deltaMs);
	void DrawBackground();
	void DrawChatText();
	void DrawPrompt();

private:
    BaseUI* mUI;

	std::shared_ptr<Visual> mVisualBackground;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;
	std::shared_ptr<BlendState> mBlendState;

	ChatBackend* mChatBackend;

	// current screen size
	Vector2<unsigned int> mScreenSize;

	// used to compute how much time passed since last animate()
	unsigned int mAnimateTimeOld;

	// should the console be opened or closed?
	bool mOpen = false;
	// should it close after you press enter?
	bool mCloseOnEnter = false;
	// current console height [pixels]
	int mHeight = 0;
	// desired height [pixels]
	float mDesiredHeight = 0.0f;
	// desired height [screen height fraction]
	float mDesiredHeightFraction = 0.0f;
	// console open/close animation speed [screen height fraction / second]
	float mHeightSpeed = 5.0f;
	// if nonzero, opening the console is inhibited [milliseconds]
	unsigned int mOpenInhibited = 0;

	// cursor blink frame (16-bit value)
	// cursor is off during [0,32767] and on during [32768,65535]
	unsigned int mCursorBlink = 0;
	// cursor blink speed [on/off toggles / second]
	float mCursorBlinkSpeed = 0.0f;
	// cursor height [line height]
	float mCursorHeight = 0.0f;

	// background texture
	std::shared_ptr<Texture2> mBackground = nullptr;
	// background color (including alpha)
	SColor mBackgroundColor = SColor(255, 0, 0, 0);

	// font
	std::shared_ptr<BaseUIFont> mFont = nullptr;
	Vector2<unsigned int> mFontSize;
};

#endif