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

#ifndef CHAT_H
#define CHAT_H

#include "GameEngineStd.h"

#include "Core/Threading/Thread.h"
#include "Core/Utility/EnrichedString.h"

#include <ctime>


enum ChatEventType 
{
    CET_CHAT,
    CET_NICK_ADD,
    CET_NICK_REMOVE,
    CET_TIME_INFO,
};

enum ChatMessageType
{
    CHATMESSAGE_TYPE_RAW = 0,
    CHATMESSAGE_TYPE_NORMAL = 1,
    CHATMESSAGE_TYPE_ANNOUNCE = 2,
    CHATMESSAGE_TYPE_SYSTEM = 3,
    CHATMESSAGE_TYPE_MAX = 4,
};

struct ChatMessage
{
    ChatMessage(const std::wstring &m = L"") : message(m) {}

    ChatMessage(ChatMessageType t, const std::wstring& m, 
        const std::wstring& s = L"", std::time_t ts = std::time(0)) : 
        type(t), message(m), sender(s), timestamp(ts)
    {
    }

    ChatMessageType type = CHATMESSAGE_TYPE_RAW;
    std::wstring message = L"";
    std::wstring sender = L"";
    std::time_t timestamp = std::time(0);
};


class ChatEvent
{
protected:
    ChatEvent(ChatEventType aType) { type = aType; }
public:
    ChatEventType type;
};

struct ChatEventTimeInfo : public ChatEvent
{
    ChatEventTimeInfo(unsigned int aGameTime, unsigned int aTime) :
        ChatEvent(CET_TIME_INFO), gameTime(aGameTime), time(aTime)
    {
    
    }

    unsigned int gameTime;
    unsigned int time;
};

struct ChatEventNick : public ChatEvent 
{
    ChatEventNick(ChatEventType aType, const std::string& aNick) :
        ChatEvent(aType), nick(aNick)
    {
    
    }

    std::string nick;
};

struct ChatEventChat : public ChatEvent 
{
    ChatEventChat(const std::string& aNick, const std::wstring& anEvtMsg) :
        ChatEvent(CET_CHAT), nick(aNick), evtMsg(anEvtMsg)
    {
    
    }

    std::string nick;
    std::wstring evtMsg;
};

struct ChatInterface 
{
    MutexedQueue<ChatEvent *> commandQueue; // chat backend --> server
    MutexedQueue<ChatEvent *> outgoingQueue; // server --> chat backend
};


// Chat console related classes
struct ChatLine
{
	// age in seconds
	float age = 0.0f;
	// name of sending player, or empty if sent by server
	EnrichedString name;
	// message text
	EnrichedString text;

	ChatLine(const std::wstring& aName, const std::wstring& aText) :
		name(aName), text(aText)
	{

	}

	ChatLine(const EnrichedString& aName, const EnrichedString& aText) :
		name(aName), text(aText)
	{

	}
};

struct ChatFormattedFragment
{
	// text string
	EnrichedString text;
	// starting column
	unsigned int column;
	// formatting
	//u8 bold:1;
};

struct ChatFormattedLine
{
	// Array of text fragments
	std::vector<ChatFormattedFragment> fragments;
	// true if first line of one formatted ChatLine
	bool first;
};

class ChatBuffer
{
public:
	ChatBuffer(unsigned int scrollback);
	~ChatBuffer() = default;

	// Append chat line
	// Removes oldest chat line if scrollback size is reached
	void AddLine(const std::wstring& name, const std::wstring& text);

	// Remove all chat lines
	void Clear();

	// Get number of lines currently in buffer.
	size_t GetLineCount() const;
	// Get reference to i-th chat line.
	const ChatLine& GetLine(unsigned int index) const;

	// Increase each chat line's age by deltaMs.
	void Step(float deltaMs);
	// Delete oldest N chat lines.
	void DeleteOldest(unsigned int count);
	// Delete lines older than maxAge.
	void DeleteByAge(float maxAge);

	// Get number of rows, 0 if reformat has not been called yet.
	unsigned int GetRows() const;
	// Update console size and reformat all formatted lines.
	void Reformat(unsigned int cols, unsigned int rows);
	// Get formatted line for a given row (0 is top of screen).
	// Only valid after reformat has been called at least once
	const ChatFormattedLine& GetFormattedLine(unsigned int row) const;
	// Scrolling in formatted buffer (relative)
	// positive rows == scroll up, negative rows == scroll down
	void Scroll(int rows);
	// Scrolling in formatted buffer (absolute)
	void ScrollAbsolute(int scroll);
	// Scroll to bottom of buffer (newest)
	void ScrollBottom();
	// Scroll to top of buffer (oldest)
	void ScrollTop();

	// Format a chat line for the given number of columns.
	// Appends the formatted lines to the destination array and
	// returns the number of formatted lines.
	unsigned int FormatChatLine(const ChatLine& line, unsigned int cols,
			std::vector<ChatFormattedLine>& destination) const;

	void Resize(unsigned int scrollback);

protected:

	int GetTopScrollPosition() const;
	int GetBottomScrollPosition() const;

private:
	// Scrollback size
	unsigned int mScrollback;
	// Array of unformatted chat lines
	std::vector<ChatLine> mUnformatted;

	// Number of character columns in console
	unsigned int mCols = 0;
	// Number of character rows in console
	unsigned int mRows = 0;
	// Scroll position (console's top line index into mFormatted)
	int mScroll = 0;
	// Array of formatted lines
	std::vector<ChatFormattedLine> mFormatted;
	// Empty formatted line, for error returns
	ChatFormattedLine mEmptyFormattedLine;
};

class ChatPrompt
{
public:
	ChatPrompt(const std::wstring& prompt, unsigned int historyLimit);
	~ChatPrompt() = default;

	// Input character or string
	void Input(wchar_t ch);
	void Input(const std::wstring& str);

	// Add a string to the history
	void AddToHistory(const std::wstring& line);

	// Get current line
    std::wstring GetLine() const { return mLine; }

	// Get section of line that is currently selected
    std::wstring GetSelection() const { return mLine.substr(mCursor, mCursorLen); }

	// Clear the current line
	void Clear();

	// Replace the current line with the given text
	std::wstring Replace(const std::wstring& line);

	// Select previous command from history
	void HistoryPrevious();
	// Select next command from history
	void HistoryNext();

	// Nick completion
	void NickCompletion(const std::list<std::string>& names, bool backwards);

	// Update console size and reformat the visible portion of the prompt
	void Reformat(unsigned int cols);
	// Get visible portion of the prompt.
	std::wstring GetVisiblePortion() const;
	// Get cursor position (relative to visible portion). -1 if invalid
	int GetVisibleCursorPosition() const;
	// Get length of cursor selection
	int GetCursorLength() const { return mCursorLen; }

	// Cursor operations
	enum CursorOp 
    {
		CURSOROP_MOVE,
		CURSOROP_SELECT,
		CURSOROP_DELETE
	};

	// Cursor operation direction
	enum CursorOpDir 
    {
		CURSOROP_DIR_LEFT,
		CURSOROP_DIR_RIGHT
	};

	// Cursor operation scope
	enum CursorOpScope 
    {
		CURSOROP_SCOPE_CHARACTER,
		CURSOROP_SCOPE_WORD,
		CURSOROP_SCOPE_LINE,
		CURSOROP_SCOPE_SELECTION
	};

	// Cursor operation
	// op specifies whether it's a move or delete operation
	// dir specifies whether the operation goes left or right
	// scope specifies how far the operation will reach (char/word/line)
	// Examples:
	//   CursorOperation(CURSOROP_MOVE, CURSOROP_DIR_RIGHT, CURSOROP_SCOPE_LINE)
	//     moves the cursor to the end of the line.
	//   CursorOperation(CURSOROP_DELETE, CURSOROP_DIR_LEFT, CURSOROP_SCOPE_WORD)
	//     deletes the word to the left of the cursor.
	void CursorOperation(CursorOp op, CursorOpDir dir, CursorOpScope scope);

protected:
	// set m_view to ensure that 0 <= m_view <= mCursor < m_view + mCols
	// if line can be fully shown, set m_view to zero
	// else, also ensure m_view <= mLine.size() + 1 - mCols
	void ClampView();

private:
	// Prompt prefix
	std::wstring mPrompt = L"";
	// Currently edited line
    std::wstring mLine = L"";
	// History buffer
    std::vector<std::wstring> mHistory;
	// History index (0 <= mHistoryIndex <= mHistory.size())
	unsigned int mHistoryIndex = 0;
	// Maximum number of history entries
	unsigned int mHistoryLimit;

	// Number of columns excluding columns reserved for the prompt
	int mCols = 0;
	// Start of visible portion (index into mLine)
	int mView = 0;
	// Cursor (index into mLine)
	int mCursor = 0;
	// Cursor length (length of selected portion of line)
	int mCursorLen = 0;

	// Last nick completion start (index into mLine)
	int mNickCompletionStart = 0;
	// Last nick completion start (index into mLine)
	int mNickCompletionEnd = 0;
};

class ChatBackend
{
public:
	ChatBackend();
	~ChatBackend() = default;

	// Add chat message
	void AddMessage(const std::wstring& name, std::wstring text);
	// Parse and add unparsed chat message
	void AddUnparsedMessage(std::wstring line);

	// Get the console buffer
	ChatBuffer& GetConsoleBuffer();
	// Get the recent messages buffer
	ChatBuffer& GetRecentBuffer();
	// Concatenate all recent messages
	EnrichedString GetRecentChat() const;
	// Get the console prompt
	ChatPrompt& GetPrompt();

	// Reformat all buffers
	void Reformat(unsigned int cols, unsigned int rows);

	// Clear all recent messages
	void ClearRecentChat();

	// Age recent messages
	void Step(float deltaMs);

	// Scrolling
	void Scroll(int rows);
	void ScrollPageDown();
	void ScrollPageUp();

	// Resize recent buffer based on settings
	void ApplySettings();

private:
	ChatBuffer mConsoleBuffer;
	ChatBuffer mRecentBuffer;
	ChatPrompt mPrompt;
};

#endif