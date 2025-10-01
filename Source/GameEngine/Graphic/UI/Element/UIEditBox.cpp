// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIEditBox.h"

#include "Core/OS/OS.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"

//! constructor
UIEditBox::UIEditBox(BaseUI* ui, int id, RectangleShape<2, int> rectangle,
    const wchar_t* text, bool border, bool writable) : BaseUIEditBox(id, rectangle), 
    mUI(ui), mWritable(writable), mOverwriteMode(false), mMouseMarking(false), mBorder(border),
    mBackground(true), mBackgroundColorUsed(false), mOverrideColorEnabled(false), 
    mOverrideColor(SColor(101, 255, 255, 255)), mOverrideFont(0), 
    mMarkBegin(0), mMarkEnd(0), mLastBreakFont(0), mBlinkStartTime(0), 
    mCursorPos(0), mHScrollPos(0), mVScrollPos(0), mScrollBarWidth(0), mMax(0), 
    mWordWrap(false), mMultiLine(false), mAutoScroll(true), mPasswordBox(false), mPasswordChar(L'*'), 
    mHAlign(UIA_UPPERLEFT), mVAlign(UIA_CENTER), mFrameRect(rectangle)
{
	mText = text;

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

        std::shared_ptr<VisualEffect> effect = std::make_shared<ColorEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
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
}


//! destructor
UIEditBox::~UIEditBox()
{

}

//! initialize editbox
void UIEditBox::OnInit()
{
	// this element can be tabbed to
	SetTabStop(true);
	SetTabOrder(-1);

    CreateVScrollBar();

	CalculateFrameRect();
	BreakText();

	CalculateScrollPosition();
}


//! Sets another skin independent font.
void UIEditBox::SetOverrideFont(const std::shared_ptr<BaseUIFont>& font)
{
	if (mOverrideFont == font)
		return;

	mOverrideFont = font;
	BreakText();
}

//! Gets the override font (if any)
const std::shared_ptr<BaseUIFont>& UIEditBox::GetOverrideFont() const
{
	return mOverrideFont;
}

//! Get the font which is used right now for drawing
const std::shared_ptr<BaseUIFont>& UIEditBox::GetActiveFont() const
{
	if ( mOverrideFont )
		return mOverrideFont;

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (skin)
		return skin->GetFont();

	return nullptr;
}

//! Sets backgroundColor
void UIEditBox::SetBackgroundColor(SColor color)
{
    mBackgroundColorUsed = true;
    mBackgroundColor = color;
}

//! Sets another color for the text.
void UIEditBox::SetOverrideColor(SColor color)
{
	mOverrideColor = color;
	mOverrideColorEnabled = true;
}


SColor UIEditBox::GetOverrideColor() const
{
	return mOverrideColor;
}


//! Turns the border on or off
void UIEditBox::SetDrawBorder(bool border)
{
	mBorder = border;
}

//! Sets whether to draw the background
void UIEditBox::SetDrawBackground(bool draw)
{
	mBackground = draw;
}

//! Sets if the text should use the overide color or the color in the gui skin.
void UIEditBox::EnableOverrideColor(bool enable)
{
	mOverrideColorEnabled = enable;
}

bool UIEditBox::IsOverrideColorEnabled() const
{
	return mOverrideColorEnabled;
}

//! Enables or disables word wrap
void UIEditBox::SetWordWrap(bool enable)
{
	mWordWrap = enable;
	BreakText();
}

void UIEditBox::UpdateAbsolutePosition()
{
	RectangleShape<2, int> oldAbsoluteRect(mAbsoluteRect);
	BaseUIElement::UpdateAbsolutePosition();

	if ( oldAbsoluteRect != mAbsoluteRect )
	{
		CalculateFrameRect();
		BreakText();
		CalculateScrollPosition();
	}
}


//! Checks if word wrap is enabled
bool UIEditBox::IsWordWrapEnabled() const
{
	return mWordWrap;
}


//! Enables or disables newlines.
void UIEditBox::SetMultiLine(bool enable)
{
	mMultiLine = enable;
	BreakText();
}


//! Checks if multi line editing is enabled
bool UIEditBox::IsMultiLineEnabled() const
{
	return mMultiLine;
}


void UIEditBox::SetPasswordBox(bool passwordBox, wchar_t passwordChar)
{
	mPasswordBox = passwordBox;
	if (mPasswordBox)
	{
		mPasswordChar = passwordChar;
		SetMultiLine(false);
		SetWordWrap(false);
		mBrokenText.clear();
	}
}


bool UIEditBox::IsPasswordBox() const
{
	return mPasswordBox;
}


//! Sets text justification
void UIEditBox::SetTextAlignment(UIAlignment horizontal, UIAlignment vertical)
{
	mHAlign = horizontal;
	mVAlign = vertical;
}


//! called if an event happened.
bool UIEditBox::OnEvent(const Event& evt)
{
	if (IsEnabled())
	{
		switch(evt.mEventType)
		{
		    case ET_UI_EVENT:
			    if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST)
			    {
				    if (evt.mUIEvent.mCaller == this)
				    {
					    mMouseMarking = false;
					    SetTextMarkers(0,0);
				    }
			    }
			    break;
		    case ET_KEY_INPUT_EVENT:
			    if (ProcessKey(evt))
				    return true;
			    break;
		    case ET_MOUSE_INPUT_EVENT:
			    if (ProcessMouse(evt))
				    return true;
			    break;

            case ET_STRING_INPUT_EVENT:
                InputString(evt.mStringInput.mStr);
                return true;

		    default:
			    break;
		}
	}

	return BaseUIElement::OnEvent(evt);
}


bool UIEditBox::ProcessKey(const Event& evt)
{
	if (!evt.mKeyInput.mPressedDown)
		return false;

	bool textChanged = false;
	int newMarkBegin = mMarkBegin;
	int newMarkEnd = mMarkEnd;

	// control shortcut handling
	if (evt.mKeyInput.mControl)
	{
		// german backlash '\' entered with control + '?'
		if (evt.mKeyInput.mChar == '\\' )
		{
			InputChar(evt.mKeyInput.mChar);
			return true;
		}

		switch(evt.mKeyInput.mKey)
		{
			case KEY_KEY_A:
				// select all
				newMarkBegin = 0;
				newMarkEnd = (int)mText.size();
				break;
			case KEY_KEY_C:
				// copy to clipboard
				if (!mPasswordBox && mMarkBegin != mMarkEnd)
				{
					const int realmbgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
					const int realmend = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;

					std::wstring s;
					s = mText.substr(realmbgn, realmend - realmbgn);
					//Operator->copyToClipboard(s.c_str());
				}
				break;
			case KEY_KEY_X:
				// cut to the clipboard
				if (!mPasswordBox && mMarkBegin != mMarkEnd)
				{
					const int realmbgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
					const int realmend = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;

					// copy
					std::wstring s;
					s = mText.substr(realmbgn, realmend - realmbgn);
					//Operator->copyToClipboard(s.c_str());

					if (IsEnabled())
					{
						// delete
						std::wstring s;
						s = mText.substr(0, realmbgn);
						s.append( mText.substr(realmend, mText.size() - realmend) );
						mText = s;

						mCursorPos = realmbgn;
						newMarkBegin = 0;
						newMarkEnd = 0;
						textChanged = true;
					}
				}
				break;
			case KEY_KEY_V:
			{
				if (!IsEnabled())
					break;

				// paste from the clipboard
				const int realmbgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
				const int realmend = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;

				// add new character
				const char* p = nullptr; // = Operator->getTextFromClipboard();
				if (p)
				{
					// TODO: we should have such a function in std::string
					size_t lenOld = strlen(p);
					wchar_t *ws = new wchar_t[lenOld + 1];
					size_t len = mbstowcs(ws, p, lenOld);
					ws[len] = 0;
					std::wstring widep(ws);
					delete[] ws;

					if (mMarkBegin == mMarkEnd)
					{
						// insert text
						std::wstring s = mText.substr(0, mCursorPos);
						s.append(widep);
						s.append(mText.substr(mCursorPos, mText.size() - mCursorPos));

						if (!mMax || s.size() <= mMax) // thx to Fish FH for fix
						{
							mText = s;
							s = widep;
							mCursorPos += (int)s.size();
						}
					}
					else
					{
						// replace text
						std::wstring s = mText.substr(0, realmbgn);
						s.append(widep);
						s.append(mText.substr(realmend, mText.size() - realmend));

						if (!mMax || s.size() <= mMax)  // thx to Fish FH for fix
						{
							mText = s;
							s = widep;
							mCursorPos = realmbgn + (int)s.size();
						}
					}

					newMarkBegin = 0;
					newMarkEnd = 0;
					textChanged = true;
				}
				break;
			}
			case KEY_HOME:
				// move/highlight to start of text
				if (evt.mKeyInput.mShift)
				{
					newMarkEnd = mCursorPos;
					newMarkBegin = 0;
					mCursorPos = 0;
				}
				else
				{
					mCursorPos = 0;
					newMarkBegin = 0;
					newMarkEnd = 0;
				}
				break;
			case KEY_END:
				// move/highlight to end of text
				if (evt.mKeyInput.mShift)
				{
					newMarkBegin = mCursorPos;
					newMarkEnd = (int)mText.size();
					mCursorPos = 0;
				}
				else
				{
					mCursorPos = (int)mText.size();
					newMarkBegin = 0;
					newMarkEnd = 0;
				}
				break;
			default:
				return false;
		}
	}
	// default keyboard handling
	else
	{
		switch (evt.mKeyInput.mKey)
		{
			case KEY_END:
			{
				int p = (int)mText.size();
				if (mWordWrap || mMultiLine)
				{
					p = GetLineFromPosition(mCursorPos);
					p = mBrokenTextPositions[p] + (int)mBrokenText[p].size();
					if (p > 0 && (mText[p - 1] == L'\r' || mText[p - 1] == L'\n'))
						p -= 1;
				}

				if (evt.mKeyInput.mShift)
				{
					if (mMarkBegin == mMarkEnd)
						newMarkBegin = mCursorPos;

					newMarkEnd = p;
				}
				else
				{
					newMarkBegin = 0;
					newMarkEnd = 0;
				}
				mCursorPos = p;
				mBlinkStartTime = Timer::GetTime();
			}
			break;
			case KEY_HOME:
			{
				int p = 0;
				if (mWordWrap || mMultiLine)
				{
					p = GetLineFromPosition(mCursorPos);
					p = mBrokenTextPositions[p];
				}

				if (evt.mKeyInput.mShift)
				{
					if (mMarkBegin == mMarkEnd)
						newMarkBegin = mCursorPos;
					newMarkEnd = p;
				}
				else
				{
					newMarkBegin = 0;
					newMarkEnd = 0;
				}
				mCursorPos = p;
				mBlinkStartTime = Timer::GetTime();
			}
			break;
			case KEY_RETURN:
				if (mMultiLine)
				{
					InputChar(L'\n');
				}
				else
				{
					CalculateScrollPosition();
					SendUIEvent(UIEVT_EDITBOX_ENTER);
				}
				return true;
			case KEY_LEFT:
			{
				if (evt.mKeyInput.mShift)
				{
					if (mCursorPos > 0)
					{
						if (mMarkBegin == mMarkEnd)
							newMarkBegin = mCursorPos;

						newMarkEnd = mCursorPos - 1;
					}
				}
				else
				{
					newMarkBegin = 0;
					newMarkEnd = 0;
				}

				if (mCursorPos > 0) mCursorPos--;
				mBlinkStartTime = Timer::GetTime();
				break;
			}

			case KEY_RIGHT:
			{
				if (evt.mKeyInput.mShift)
				{
					if ((int)mText.size() > mCursorPos)
					{
						if (mMarkBegin == mMarkEnd)
							newMarkBegin = mCursorPos;

						newMarkEnd = mCursorPos + 1;
					}
				}
				else
				{
					newMarkBegin = 0;
					newMarkEnd = 0;
				}

				if ((int)mText.size() > mCursorPos) mCursorPos++;
				mBlinkStartTime = Timer::GetTime();
				break;
			}
			case KEY_UP:
				if (mMultiLine || (mWordWrap && mBrokenText.size() > 1))
				{
					int lineNo = GetLineFromPosition(mCursorPos);
					int mb = (mMarkBegin == mMarkEnd) ? mCursorPos : (mMarkBegin > mMarkEnd ? mMarkBegin : mMarkEnd);
					if (lineNo > 0)
					{
						int cp = mCursorPos - mBrokenTextPositions[lineNo];
						if ((int)mBrokenText[lineNo - 1].size() < cp)
						{
							mCursorPos = mBrokenTextPositions[lineNo - 1] +
								std::max((unsigned int)1, (unsigned int)mBrokenText[lineNo - 1].size()) - 1;
						}
						else mCursorPos = mBrokenTextPositions[lineNo - 1] + cp;
					}

					if (evt.mKeyInput.mShift)
					{
						newMarkBegin = mb;
						newMarkEnd = mCursorPos;
					}
					else
					{
						newMarkBegin = 0;
						newMarkEnd = 0;
					}
				}
				else
				{
					return false;
				}
				break;
			case KEY_DOWN:
				if (mMultiLine || (mWordWrap && mBrokenText.size() > 1))
				{
					int lineNo = GetLineFromPosition(mCursorPos);
					int mb = (mMarkBegin == mMarkEnd) ? mCursorPos : (mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd);
					if (lineNo < (int)mBrokenText.size() - 1)
					{
						int cp = mCursorPos - mBrokenTextPositions[lineNo];
						if ((int)mBrokenText[lineNo + 1].size() < cp)
						{
							mCursorPos = mBrokenTextPositions[lineNo + 1] +
								std::max((unsigned int)1, (unsigned int)mBrokenText[lineNo + 1].size()) - 1;
						}
						else mCursorPos = mBrokenTextPositions[lineNo + 1] + cp;
					}

					if (evt.mKeyInput.mShift)
					{
						newMarkBegin = mb;
						newMarkEnd = mCursorPos;
					}
					else
					{
						newMarkBegin = 0;
						newMarkEnd = 0;
					}
				}
				else
				{
					return false;
				}
				break;

			case KEY_BACK:
				if (!IsEnabled())
					break;

				if (mText.size())
				{
					std::wstring s;

					if (mMarkBegin != mMarkEnd)
					{
						// delete marked text
						const int realmbgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
						const int realmend = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;

						s = mText.substr(0, realmbgn);
						s.append(mText.substr(realmend, mText.size() - realmend));
						mText = s;

						mCursorPos = realmbgn;
					}
					else
					{
						// delete text behind cursor
						if (mCursorPos>0)
							s = mText.substr(0, mCursorPos - 1);
						else
							s = L"";
						s.append(mText.substr(mCursorPos, mText.size() - mCursorPos));
						mText = s;

						--mCursorPos;
					}

					if (mCursorPos < 0)
						mCursorPos = 0;
					mBlinkStartTime = Timer::GetTime();
					newMarkBegin = 0;
					newMarkEnd = 0;
					textChanged = true;
				}
				break;
            case KEY_INSERT:
                if (!IsEnabled())
                    break;

                mOverwriteMode = !mOverwriteMode;
                break;

			case KEY_DELETE:
				if (!IsEnabled())
					break;

				if (mText.size() != 0)
				{
					std::wstring s;

					if (mMarkBegin != mMarkEnd)
					{
						// delete marked text
						const int realmbgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
						const int realmend = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;

						s = mText.substr(0, realmbgn);
						s.append(mText.substr(realmend, mText.size() - realmend));
						mText = s;

						mCursorPos = realmbgn;
					}
					else
					{
						// delete text before cursor
						s = mText.substr(0, mCursorPos);
                        if (mCursorPos + 1 < mText.length())
                            s.append(mText.substr(mCursorPos + 1, mText.size() - mCursorPos - 1));
						mText = s;
					}

					if (mCursorPos > (int)mText.size())
						mCursorPos = (int)mText.size();

					mBlinkStartTime = Timer::GetTime();
					newMarkBegin = 0;
					newMarkEnd = 0;
					textChanged = true;
				}
				break;

			case KEY_ESCAPE:
			case KEY_TAB:
			case KEY_SHIFT:
			case KEY_F1:
			case KEY_F2:
			case KEY_F3:
			case KEY_F4:
			case KEY_F5:
			case KEY_F6:
			case KEY_F7:
			case KEY_F8:
			case KEY_F9:
			case KEY_F10:
			case KEY_F11:
			case KEY_F12:
			case KEY_F13:
			case KEY_F14:
			case KEY_F15:
			case KEY_F16:
			case KEY_F17:
			case KEY_F18:
			case KEY_F19:
			case KEY_F20:
			case KEY_F21:
			case KEY_F22:
			case KEY_F23:
			case KEY_F24:
				// ignore these keys
				return false;

			default:
				InputChar(evt.mKeyInput.mChar);
				return true;
		}
	}

	// Set new text markers
	SetTextMarkers( newMarkBegin, newMarkEnd );

	// break the text if it has changed
	if (textChanged)
	{
		BreakText();
		CalculateScrollPosition();
		SendUIEvent(UIEVT_EDITBOX_CHANGED);
	}
	else CalculateScrollPosition();

	return true;
}


void UIEditBox::InputString(const std::wstring& str)
{
    if (!IsEnabled() && !mWritable)
        return;

    std::wstring s;
    int len = (int)str.size();

    if (mMarkBegin != mMarkEnd)
    {
        // replace marked text
        const int realmbgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
        const int realmend = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;

        s = mText.substr(0, realmbgn);
        s.append(str);
        s.append(mText.substr(realmend, mText.size() - realmend));
        mText = s;
        mCursorPos = realmbgn + len;
    }
    else if (mOverwriteMode)
    {
        //check to see if we are at the end of the text
        if (mCursorPos + len < (int)mText.size())
        {
            bool isEOL = false;
            int currentEOLPos;
            for (int i = mCursorPos; i < mCursorPos + len && i < mMax; i++)
            {
                if (mText[i] == L'\n' || mText[i] == L'\r')
                {
                    isEOL = true;
                    currentEOLPos = i;
                    break;
                }
            }
            if (!isEOL || mText.size() + len <= mMax || mMax == 0)
            {
                s = mText.substr(0, mCursorPos);
                s.append(str);
                if (isEOL)
                {
                    //just keep appending to the current line
                    //This follows the behavior of other gui libraries behaviors
                    s.append(mText.substr(currentEOLPos, mText.size() - currentEOLPos));
                }
                else
                {
                    //replace the next character
                    s.append(mText.substr(mCursorPos + len, mText.size() - mCursorPos - len));
                }
                mText = s;
                mCursorPos += len;
            }
        }
        else if (mText.size() + len <= mMax || mMax == 0)
        {
            // add new character because we are at the end of the string
            s = mText.substr(0, mCursorPos);
            s.append(str);
            s.append(mText.substr(mCursorPos, mText.size() - mCursorPos - len));
            mText = s;
            mCursorPos += len;
        }
    }
    else if (mText.size() + len <= mMax || mMax == 0)
    {
        // add new character
        s = mText.substr(0, mCursorPos);
        s.append(str);
        s.append(mText.substr(mCursorPos, mText.size() - mCursorPos));
        mText = s;
        mCursorPos += len;
    }

    mBlinkStartTime = Timer::GetTime();
    SetTextMarkers(0, 0);

    BreakText();
    CalculateScrollPosition();
    SendUIEvent(UIEVT_EDITBOX_CHANGED);
}

void UIEditBox::InputChar(wchar_t c)
{
    if (c == 0)
        return;

    std::wstring s(&c, 1);
    InputString(s);
}


//! draws the element and its children
void UIEditBox::Draw()
{
	if (!IsVisible())
		return;

	const bool focus = mUI->HasFocus(shared_from_this());

	std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
	if (!skin)
		return;

    SColor defaultBGColor = SColor(0);
    if (mWritable)
        defaultBGColor = focus ? skin->GetColor(DC_FOCUSED_EDITABLE) : skin->GetColor(DC_EDITABLE);
    SColor bgColor = mBackgroundColorUsed ? mBackgroundColor : defaultBGColor;

    if (!mBorder && mBackground)
    {
        skin->Draw2DRectangle(
            bgColor, mVisualBackground, mAbsoluteRect, &mAbsoluteClippingRect);
    }

    // draw the border
    if (mBorder)
    {
        if (mWritable)
        {
            skin->Draw3DSunkenPane(bgColor, false, mBackground,
                mVisualBackground, mAbsoluteRect, &mAbsoluteClippingRect);
        }

        CalculateFrameRect();
    }

	RectangleShape<2, int> clipRect = mAbsoluteClippingRect;
    RectangleShape<2, int> localClipRect = mFrameRect;
    localClipRect.ClipAgainst(mAbsoluteClippingRect);

	// draw the text
	const std::shared_ptr<BaseUIFont>& font = GetActiveFont();

	int cursorLine = 0;
    int charCursorPos = 0;

	if (font)
	{
		if (mLastBreakFont != font)
			BreakText();

		// calculate cursor pos
		std::wstring *txtLine = &mText;
		int startPos = 0;

		std::wstring s, s2;

		// get mark position
		const bool ml = (!mPasswordBox && (mWordWrap || mMultiLine));
		const int realMarkBgn = mMarkBegin < mMarkEnd ? mMarkBegin : mMarkEnd;
		const int realMarkEnd = mMarkBegin < mMarkEnd ? mMarkEnd : mMarkBegin;
		const int hlineStart = ml ? GetLineFromPosition(realMarkBgn) : 0;
		const int hlineCount = ml ? GetLineFromPosition(realMarkEnd) - hlineStart + 1 : 1;
		const int lineCount = ml ? (int)mBrokenText.size() : 1;

		// Save the override color information.
		// Then, alter it if the edit box is disabled.
		const bool prevOver = mOverrideColorEnabled;
		const SColor prevColor = mOverrideColor;

		if (mText.size())
		{
			if (!IsEnabled() && !mOverrideColorEnabled)
			{
				mOverrideColorEnabled = true;
				mOverrideColor = skin->GetColor(DC_GRAY_TEXT);
			}

			for (int i=0; i < lineCount; ++i)
			{
				SetTextRect(i);

                // clipping test - don't draw anything outside the visible area
                RectangleShape<2, int> c = localClipRect;
                c.ClipAgainst(mCurrentTextRect);
                if (!c.IsValid())
                    continue;

				// get current line
				if (mPasswordBox)
				{
					if (mBrokenText.size() != 1)
					{
						mBrokenText.clear();
						mBrokenText.push_back(std::wstring());
					}
					if (mBrokenText[0].size() != mText.size())
					{
						mBrokenText[0] = mText;
						for (unsigned int q = 0; q < mText.size(); ++q)
						{
							mBrokenText[0] [q] = mPasswordChar;
						}
					}
					txtLine = &mBrokenText[0];
					startPos = 0;
				}
				else
				{
					txtLine = ml ? &mBrokenText[i] : &mText;
					startPos = ml ? mBrokenTextPositions[i] : 0;
				}

				// draw normal text
				font->Draw(txtLine->c_str(), mCurrentTextRect,
					mOverrideColorEnabled ? mOverrideColor : skin->GetColor(DC_BUTTON_TEXT),
					false, true, &localClipRect);

				// draw mark and marked text
				if (focus && mMarkBegin != mMarkEnd && 
					i >= hlineStart && i < hlineStart + hlineCount)
				{
					int markBegin = 0, markEnd = 0;
					int lineStartPos = 0, lineEndPos = (int)txtLine->size();

					if (i == hlineStart)
					{
						// highlight start is on this line
						s = txtLine->substr(0, realMarkBgn - startPos);
                        markBegin = font->GetDimension(s)[0];

						lineStartPos = realMarkBgn - startPos;
					}
					if (i == hlineStart + hlineCount - 1)
					{
						// highlight end is on this line
						s2 = txtLine->substr(0, realMarkEnd - startPos);
                        markEnd = font->GetDimension(s2)[0];

						lineEndPos = (int)s2.size();
					}
					else markEnd = font->GetDimension(txtLine->c_str())[0];

                    if (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0] < localClipRect.GetVertice(RVP_UPPERLEFT)[0])
                    {
                        int markOffset = font->GetDimension(*txtLine,
                            localClipRect.GetVertice(RVP_UPPERLEFT)[0] - mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0])[0];

                        mCurrentTextRect.mCenter[0] = localClipRect.GetVertice(RVP_UPPERLEFT)[0] + 
                            markBegin - markOffset + (markEnd - markBegin) / 2;
                        mCurrentTextRect.mExtent[0] = markEnd - markBegin;
                    }
                    else
                    {
                        mCurrentTextRect.mCenter[0] = mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0] + 
                            markBegin + (markEnd - markBegin) / 2;
                        mCurrentTextRect.mExtent[0] = markEnd - markBegin;
                    }

                    c = mCurrentTextRect;
                    c.ClipAgainst(localClipRect);
                    if (c.IsValid())
                        skin->Draw2DRectangle(skin->GetColor(DC_HIGH_LIGHT), mVisualBackground, c, &clipRect);

                    // draw marked text
                    s = txtLine->substr(lineStartPos, lineEndPos - lineStartPos);

					if (s.size())
						font->Draw(s.c_str(), mCurrentTextRect,
							mOverrideColorEnabled ? mOverrideColor : skin->GetColor(DC_HIGH_LIGHT_TEXT),
							false, true, &localClipRect);
				}
			}

			// Return the override color information to its previous settings.
			mOverrideColorEnabled = prevOver;
			mOverrideColor = prevColor;
		}

		// draw cursor
		if (IsEnabled() && mWritable)
		{
			if (mMultiLine || mWordWrap)
			{
				cursorLine = GetLineFromPosition(mCursorPos);
				txtLine = &mBrokenText[cursorLine];
				startPos = mBrokenTextPositions[cursorLine];
			}
			s = txtLine->substr(0, mCursorPos - startPos);
            charCursorPos = font->GetDimension(s)[0];

			if (focus && (Timer::GetTime() - mBlinkStartTime) % 700 < 350)
			{
				SetTextRect(cursorLine);
				mCurrentTextRect.mCenter[0] += charCursorPos / 2;
                mCurrentTextRect.mExtent[0] -= charCursorPos;

                if (mOverwriteMode)
                {
                    std::wstring character = mText.substr(mCursorPos, 1);
                    int mEnd = font->GetDimension(character)[0];
                    //Make sure the cursor box has at least some width to it
                    if (mEnd <= 0) mEnd = font->GetDimension(L"_")[0];
                    
                    Vector2<int> upperLeftCorner = mCurrentTextRect.GetVertice(RVP_UPPERLEFT);
                    mCurrentTextRect.mExtent[0] = mEnd - upperLeftCorner[0];
                    mCurrentTextRect.mCenter[0] = upperLeftCorner[0] + mCurrentTextRect.mExtent[0] / 2;
                    skin->Draw2DRectangle(skin->GetColor(DC_HIGH_LIGHT), mVisualBackground, mCurrentTextRect, &clipRect);
                    font->Draw(character.c_str(), mCurrentTextRect,
                        mOverrideColorEnabled ? mOverrideColor : skin->GetColor(DC_HIGH_LIGHT_TEXT), 
                        false, true, &localClipRect);
                }
                else
                {
                    font->Draw(L"_", mCurrentTextRect,
                        mOverrideColorEnabled ? mOverrideColor : skin->GetColor(DC_BUTTON_TEXT), 
                        false, true, &localClipRect);
                }
			}
		}
	}

	// draw children
	BaseUIElement::Draw();
}


//! Sets the new caption of this element.
void UIEditBox::SetText(const wchar_t* text)
{
	mText = text;
	if (mCursorPos > (int)mText.size())
		mCursorPos = (int)mText.size();
	mHScrollPos = 0;
	BreakText();
}


void UIEditBox::SetWritable(bool canWriteText)
{
    mWritable = canWriteText;
}


//! Enables or disables automatic scrolling with cursor position
//! \param enable: If set to true, the text will move around with the cursor position
void UIEditBox::SetAutoScroll(bool enable)
{
	mAutoScroll = enable;
}


//! Checks to see if automatic scrolling is enabled
//! \return true if automatic scrolling is enabled, false if not
bool UIEditBox::IsAutoScrollEnabled() const
{
	return mAutoScroll;
}


//! Gets the area of the text in the edit box
//! \return Returns the size in pixels of the text
Vector2<int> UIEditBox::GetTextDimension()
{
	RectangleShape<2, int> ret;

	SetTextRect(0);
	ret = mCurrentTextRect;

	for (unsigned int i=1; i < mBrokenText.size(); ++i)
	{
		SetTextRect(i);
        ret.AddInternalPoint(mCurrentTextRect.GetVertice(RVP_UPPERLEFT));
        ret.AddInternalPoint(mCurrentTextRect.GetVertice(RVP_LOWERRIGHT));
	}

	return ret.mExtent;
}


//! Sets the maximum amount of characters which may be entered in the box.
//! \param max: Maximum amount of characters. If 0, the character amount is
//! infinity.
void UIEditBox::SetMax(unsigned int max)
{
	mMax = max;

	if (mText.size() > mMax && mMax != 0)
		mText = mText.substr(0, mMax);
}


//! Returns maximum amount of characters, previously set by setMax();
unsigned int UIEditBox::GetMax() const
{
	return mMax;
}


bool UIEditBox::ProcessMouse(const Event& evt)
{
	switch(evt.mMouseInput.mEvent)
	{
		case MIE_LMOUSE_LEFT_UP:
			if (mUI->HasFocus(shared_from_this()))
			{
				mCursorPos = GetCursorPosition(evt.mMouseInput.X, evt.mMouseInput.Y);
				if (mMouseMarking)
					SetTextMarkers( mMarkBegin, mCursorPos );

				mMouseMarking = false;
				CalculateScrollPosition();
				return true;
			}
			break;
		case MIE_MOUSE_MOVED:
			{
				if (mMouseMarking)
				{
					mCursorPos = GetCursorPosition(evt.mMouseInput.X, evt.mMouseInput.Y);
					SetTextMarkers( mMarkBegin, mCursorPos );
					CalculateScrollPosition();
					return true;
				}
			}
			break;
		case MIE_LMOUSE_PRESSED_DOWN:
			if (!mUI->HasFocus(shared_from_this()))
			{
				mBlinkStartTime = Timer::GetTime();
				mMouseMarking = true;
				mCursorPos = GetCursorPosition(evt.mMouseInput.X, evt.mMouseInput.Y);
				SetTextMarkers(mCursorPos, mCursorPos );
				CalculateScrollPosition();
				return true;
			}
			else
			{
				if (mAbsoluteRect.IsPointInside(Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y}))
				{
					// move cursor
					mCursorPos = GetCursorPosition(evt.mMouseInput.X, evt.mMouseInput.Y);
					int newMarkBegin = mMarkBegin;
					if (!mMouseMarking)
						newMarkBegin = mCursorPos;

					mMouseMarking = true;
					SetTextMarkers( newMarkBegin, mCursorPos);
					CalculateScrollPosition();
					return true;
				}
                else return false;
			}
        case MIE_MOUSE_WHEEL:
            if (mVScrollBar && mVScrollBar->IsVisible()) 
            {
                int pos = mVScrollBar->GetPosition();
                int step = mVScrollBar->GetSmallStep();
                mVScrollBar->SetPosition((int)(pos - evt.mMouseInput.mWheel * step));
                return true;
            }
            break;
		default:
			break;
	}

	return false;
}


int UIEditBox::GetCursorPosition(int x, int y)
{
	const std::shared_ptr<BaseUIFont>& font = GetActiveFont();

	const unsigned int lineCount = (mWordWrap || mMultiLine) ? (unsigned int)mBrokenText.size() : 1;

	std::wstring *txtLine=0;
	int startPos=0;
	x+=6;

	for (unsigned int i=0; i < lineCount; ++i)
	{
		SetTextRect(i);
		if (i == 0 && y < mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1])
			y = mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1];
		if (i == lineCount - 1 && y > mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1])
			y = mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1];

		// is it inside this region?
		if (y >= mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1] &&
			y <= mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1])
		{
			// we've found the clicked line
			txtLine = (mWordWrap || mMultiLine) ? &mBrokenText[i] : &mText;
			startPos = (mWordWrap || mMultiLine) ? mBrokenTextPositions[i] : 0;
			break;
		}
	}

    if (x < mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0])
        x = mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0];

	if ( !txtLine )
		return 0;

	// click was on or left of the line
    unsigned int idx = font->GetLength(*txtLine, 
        x - mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0]);
    return idx + startPos;
}


//! Breaks the single text line.
void UIEditBox::BreakText()
{
	if ((!mWordWrap && !mMultiLine))
		return;

	mBrokenText.clear(); // need to reallocate :/
	mBrokenTextPositions.resize(0);

	const std::shared_ptr<BaseUIFont>& font = GetActiveFont();
	if (!font)
		return;

	mLastBreakFont = font;

	std::wstring line;
	std::wstring word;
	std::wstring whitespace;
	int lastLineStart = 0;
	int size = (int)mText.size();
	int length = 0;
	int elWidth = mRelativeRect.mExtent[0] - mScrollBarWidth - 10;
	wchar_t c;

	for (int i=0; i<size; ++i)
	{
		c = mText[i];
		bool lineBreak = false;

		if (c == L'\r') // Mac or Windows breaks
		{
			lineBreak = true;
			c = 0;
			if (mText[i+1] == L'\n') // Windows breaks
			{
				// TODO: I (Michael) think that we shouldn't change the text given by the user for whatever reason.
				// Instead rework the cursor positioning to be able to handle this (but not in stable release
				// branch as users might already expect this behavior).
				mText.erase(i+1);
				--size;
				if ( mCursorPos > i )
					--mCursorPos;
			}
		}
		else if (c == L'\n') // Unix breaks
		{
			lineBreak = true;
			c = 0;
		}

		// don't break if we're not a multi-line edit box
		if (!mMultiLine)
			lineBreak = false;

		if (c == L' ' || c == 0 || i == (size-1))
		{
			// here comes the next whitespace, look if
			// we can break the last word to the next line
			// We also break whitespace, otherwise cursor would vanish beside the right border.
			int whitelgth = font->GetDimension(whitespace)[0];
			int worldlgth = font->GetDimension(word)[0];

			if (mWordWrap && length + worldlgth + whitelgth > elWidth && line.size() > 0)
			{
				// break to next line
				length = worldlgth;
				mBrokenText.push_back(line);
				mBrokenTextPositions.push_back(lastLineStart);
				lastLineStart = i - (int)word.size();
				line = word;
			}
			else
			{
				// add word to line
				line += whitespace;
				line += word;
				length += whitelgth + worldlgth;
			}

			word = L"";
			whitespace = L"";

			if ( c )
				whitespace += c;

			// compute line break
			if (lineBreak)
			{
				line += whitespace;
				line += word;
				mBrokenText.push_back(line);
				mBrokenTextPositions.push_back(lastLineStart);
				lastLineStart = i+1;
				line = L"";
				word = L"";
				whitespace = L"";
				length = 0;
			}
		}
		else
		{
			// yippee this is a word..
			word += c;
		}
	}

	line += whitespace;
	line += word;
	mBrokenText.push_back(line);
	mBrokenTextPositions.push_back(lastLineStart);
}

// TODO: that function does interpret VAlign according to line-index (indexed line is placed on top-center-bottom)
// but HAlign according to line-width (pixels) and not by row.
// Intuitively I suppose HAlign handling is better as VScrollPos should handle the line-scrolling.
// But please no one change this without also rewriting (and this time fucking testing!!!) 
// autoscrolling (I noticed this when fixing the old autoscrolling).
void UIEditBox::SetTextRect(int line)
{
	if ( line < 0 )
		return;

	const std::shared_ptr<BaseUIFont>& font = GetActiveFont();
	if (!font)
		return;

	Vector2<int> d;

	// get text dimension
	const unsigned int lineCount = (mWordWrap || mMultiLine) ? (unsigned int)mBrokenText.size() : 1;

	if (mWordWrap || mMultiLine)
	{
		d = font->GetDimension(mBrokenText[line]);
	}
	else
	{
		d = font->GetDimension(mText);
		d[1] = mAbsoluteRect.mExtent[1];
	}

    // justification
    switch (mHAlign) 
    {
        case UIA_CENTER:
            // align to h centre
            mCurrentTextRect.mExtent[0] = d[0];
            mCurrentTextRect.mCenter[0] = mFrameRect.mExtent[0] / 2;
            break;
        case UIA_LOWERRIGHT:
            // align to right edge
            mCurrentTextRect.mExtent[0] = d[0];
            mCurrentTextRect.mCenter[0] = mFrameRect.mExtent[0] - d[0] / 2;
            break;
        default:
            // align to left edge
            mCurrentTextRect.mExtent[0] = d[0];
            mCurrentTextRect.mCenter[0] = d[0] / 2;
    }

    switch (mVAlign) 
    {
        case UIA_CENTER:
        {
            // align to v centre
            int lower = mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1];
            int upper = (mFrameRect.mExtent[1] / 2) - (lineCount * d[1]) / 2 + d[1] * line;

            mCurrentTextRect.mExtent[1] = lower - upper;
            mCurrentTextRect.mCenter[1] = upper + (lower - upper) / 2;
        }
        break;
        case UIA_LOWERRIGHT:
        {
            // align to bottom edge
            int lower = mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1];
            int upper = mFrameRect.mExtent[1] - lineCount * d[1] + d[1] * line;

            mCurrentTextRect.mExtent[1] = lower - upper;
            mCurrentTextRect.mCenter[1] = upper + (lower - upper) / 2;
        }
        break;
        default:
        {
            // align to top edge
            int lower = mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1];
            int upper = d[1] * line;

            mCurrentTextRect.mExtent[1] = lower - upper;
            mCurrentTextRect.mCenter[1] = upper + (lower - upper) / 2;
        }
        break;
    }
    
    mCurrentTextRect.mCenter[1] = mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1] - mVScrollPos + d[1] / 2;
    mCurrentTextRect.mExtent[1] = d[1];
    mCurrentTextRect.mCenter[0] -= mHScrollPos;

    mCurrentTextRect.mCenter += mFrameRect.GetVertice(RVP_UPPERLEFT);
}


int UIEditBox::GetLineFromPosition(int pos)
{
	if (!mWordWrap && !mMultiLine)
		return 0;

	int i=0;
	while (i < (int)mBrokenTextPositions.size())
	{
		if (mBrokenTextPositions[i] > pos)
			return i-1;
		++i;
	}
	return (int)mBrokenTextPositions.size() - 1;
}

// calculate autoscroll
void UIEditBox::CalculateScrollPosition()
{
	if (!mAutoScroll)
		return;

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (!skin)
		return;

	const std::shared_ptr<BaseUIFont>& font = mOverrideFont ? mOverrideFont : skin->GetFont();
	if (!font)
		return;

	int cursLine = GetLineFromPosition(mCursorPos);
	if ( cursLine < 0 )
		return;

	SetTextRect(cursLine);
	const bool hasBrokenText = mMultiLine || mWordWrap;

	// Check horizonal scrolling
	// NOTE: Calculations different to vertical scrolling because setTextRect 
	// interprets VAlign relative to line but HAlign not relative to row
	{
		// get cursor position
		const std::shared_ptr<BaseUIFont>& font = GetActiveFont();
		if (!font)
			return;

		// get cursor area
		int cursorWidth = font->GetDimension(L"_")[0];
		std::wstring *txtLine = hasBrokenText ? &mBrokenText[cursLine] : &mText;
		int cPos = hasBrokenText ? mCursorPos - mBrokenTextPositions[cursLine] : mCursorPos;	// column
        int cStart = font->GetDimension(txtLine->substr(0, cPos))[0];		// pixels from text-start
        int cEnd = cStart + cursorWidth;
        int txtWidth = font->GetDimension(txtLine->c_str())[0];

        if (txtWidth < mFrameRect.mExtent[0]) 
        {
            // TODO: Needs a clean left and right gap removal depending on HAlign, 
            // similar to vertical scrolling tests for top/bottom. This check just fixes 
            // the case where it was most noticable (text smaller than clipping area).

            mHScrollPos = 0;
            SetTextRect(cursLine);
        }

		if (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0] + cStart < mFrameRect.GetVertice(RVP_UPPERLEFT)[0])
		{
			// cursor to the left of the clipping area
            mHScrollPos -= mFrameRect.GetVertice(RVP_UPPERLEFT)[0] -
                (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0] + cStart);
			SetTextRect(cursLine);

			// TODO: should show more characters to the left when we're scrolling left
			//	and the cursor reaches the border.
		}
		else if (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0] + cEnd > mFrameRect.GetVertice(RVP_LOWERRIGHT)[0])
		{
			mHScrollPos += 
                (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[0] + cEnd) - 
                mFrameRect.GetVertice(RVP_LOWERRIGHT)[0];
			SetTextRect(cursLine);
		}
	}

	// calculate vertical scrolling
	if (hasBrokenText)
	{
		int lineHeight = font->GetDimension(L"A")[1];
		// only up to 1 line fits?
		if ( lineHeight >= mFrameRect.mExtent[1] )
		{
			mVScrollPos = 0;
			SetTextRect(cursLine);
            int unscrolledPos = mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1];
			int pivot = mFrameRect.GetVertice(RVP_UPPERLEFT)[1];
			switch (mVAlign)
			{
				case UIA_CENTER:
					pivot += mFrameRect.mExtent[1] / 2;
					unscrolledPos += lineHeight / 2;
					break;
				case UIA_LOWERRIGHT:
					pivot += mFrameRect.mExtent[1];
					unscrolledPos += lineHeight;
					break;
				default:
					break;
			}
			mVScrollPos = unscrolledPos - pivot;
			SetTextRect(cursLine);
		}
		else
		{
			// First 2 checks are necessary when people delete lines
			SetTextRect(0);
			if (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1] > mFrameRect.GetVertice(RVP_UPPERLEFT)[1] &&
				mVAlign != UIA_LOWERRIGHT)
			{
				// first line is leaving a gap on top
				mVScrollPos = 0;
			}
			else if (mVAlign != UIA_UPPERLEFT)
			{
				int lastLine = mBrokenTextPositions.empty() ? 0 : (int)mBrokenTextPositions.size() - 1;
				SetTextRect(lastLine);
				if (mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1] <  mFrameRect.GetVertice(RVP_LOWERRIGHT)[1])
				{
					// last line is leaving a gap on bottom
                    mVScrollPos -= 
                        mFrameRect.GetVertice(RVP_LOWERRIGHT)[1] - mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1];
				}
			}

			SetTextRect(cursLine);
            if (mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1] < mFrameRect.GetVertice(RVP_UPPERLEFT)[1])
			{
				// text above valid area
				mVScrollPos -= 
                    mFrameRect.GetVertice(RVP_UPPERLEFT)[1] - mCurrentTextRect.GetVertice(RVP_UPPERLEFT)[1];
				SetTextRect(cursLine);
			}
            if (mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1] > mFrameRect.GetVertice(RVP_LOWERRIGHT)[1])
			{
				// text below valid area
				mVScrollPos += 
                    mCurrentTextRect.GetVertice(RVP_LOWERRIGHT)[1] - mFrameRect.GetVertice(RVP_LOWERRIGHT)[1];
				SetTextRect(cursLine);
			}
		}
	}

    if (mVScrollBar)
        mVScrollBar->SetPosition(mVScrollPos);
}

void UIEditBox::CreateVScrollBar()
{
    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    mScrollBarWidth = skin ? skin->GetSize(DS_SCROLLBAR_SIZE) : 16;

    RectangleShape<2, int> scrollBarRect = mFrameRect;
    scrollBarRect.mExtent[0] -= mFrameRect.mExtent[0] - mScrollBarWidth;
    scrollBarRect.mCenter[0] += (mFrameRect.mExtent[0] - mScrollBarWidth) / 2;
    mVScrollBar = std::static_pointer_cast<UIScrollBar>(
        mUI->AddScrollBar(false, true, scrollBarRect, mParent));
    
    mVScrollBar->SetVisible(false);
    mVScrollBar->SetSmallStep(1);
    mVScrollBar->SetLargeStep(1);
}

void UIEditBox::UpdateVScrollBar()
{
    if (!mVScrollBar)
        return;

    // OnScrollBarChanged(...)
    if (mVScrollBar->GetPosition() != mVScrollPos) 
    {
        int deltaScrollY = mVScrollBar->GetPosition() - mVScrollPos;
        mCurrentTextRect.mCenter[1] -= deltaScrollY;

        int scrollYMax = GetTextDimension()[1] - mFrameRect.mExtent[1];
        if (scrollYMax != mVScrollBar->GetMax()) 
        {
            // manage a newline or a deleted line
            mVScrollBar->SetMax(scrollYMax);
            mVScrollBar->SetPageSize(int(GetTextDimension()[1]));
            CalculateScrollPosition();
        }
        else 
        {
            // manage a newline or a deleted line
            mVScrollPos = mVScrollBar->GetPosition();
        }
    }

    // check if a vertical scrollbar is needed ?
    if (GetTextDimension()[1] - mFrameRect.mExtent[1] > 5) 
    {
        mFrameRect.mExtent[0] -= mScrollBarWidth;
        mFrameRect.mCenter[0] -= (int)round(mScrollBarWidth / 2.f);

        int scrollYMax = GetTextDimension()[1] - mFrameRect.mExtent[1];
        if (scrollYMax != mVScrollBar->GetMax()) 
        {
            mVScrollBar->SetMax(scrollYMax);
            mVScrollBar->SetPageSize(GetTextDimension()[1]);
        }

        if (!mVScrollBar->IsVisible())
            mVScrollBar->SetVisible(true);
    }
    else 
    {
        if (mVScrollBar->IsVisible()) 
        {
            mVScrollBar->SetVisible(false);
            mVScrollPos = 0;
            mVScrollBar->SetPosition(0);
            mVScrollBar->SetMax(1);
            mVScrollBar->SetPageSize(GetTextDimension()[1]);
        }
    }
}



void UIEditBox::CalculateFrameRect()
{
	mFrameRect = mAbsoluteRect;

	const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
	if (!skin)
		return;

	if (mBorder)
	{
		mFrameRect.mExtent[0] -= 2 * (skin->GetSize(DS_TEXT_DISTANCE_X)+1);
		mFrameRect.mExtent[1] -= 2 * (skin->GetSize(DS_TEXT_DISTANCE_Y)+2);
	}

    UpdateVScrollBar();
}

//! set text markers
void UIEditBox::SetTextMarkers(int begin, int end)
{
	if ( begin != mMarkBegin || end != mMarkEnd )
	{
		mMarkBegin = begin;
		mMarkEnd = end;

		SendUIEvent(UIEVT_EDITBOX_MARKING_CHANGED);
	}
}

//! send some gui event to parent
void UIEditBox::SendUIEvent(UIEventType type)
{
	if ( mParent )
	{
		Event e;
		e.mEventType = ET_UI_EVENT;
		e.mUIEvent.mCaller = this;
		e.mUIEvent.mElement = 0;
		e.mUIEvent.mEventType = type;

		mParent->OnEvent(e);
	}
}