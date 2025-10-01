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

#include "Chat.h"

#include "Application/Settings.h"

#include "Core/Logger/Logger.h"
#include "Core/Utility/StringUtil.h"

#include <algorithm>

ChatBuffer::ChatBuffer(unsigned int scrollback) : mScrollback(scrollback)
{
	if (mScrollback == 0)
		mScrollback = 1;
	mEmptyFormattedLine.first = true;
}

void ChatBuffer::AddLine(const std::wstring& name, const std::wstring& text)
{
	ChatLine line(name, text);
	mUnformatted.push_back(line);

	if (mRows > 0) {
		// mFormatted is valid and must be kept valid
		bool scrolledAtBottom = (mScroll == GetBottomScrollPosition());
		unsigned int numAdded = FormatChatLine(line, mCols, mFormatted);
		if (scrolledAtBottom)
			mScroll += numAdded;
	}

	// Limit number of lines by mScrollback
	if (mUnformatted.size() > mScrollback)
		DeleteOldest((unsigned int)mUnformatted.size() - mScrollback);
}

void ChatBuffer::Clear()
{
	mUnformatted.clear();
	mFormatted.clear();
	mScroll = 0;
}

size_t ChatBuffer::GetLineCount() const
{
	return mUnformatted.size();
}

const ChatLine& ChatBuffer::GetLine(unsigned int index) const
{
	LogAssert(index < GetLineCount(), "invalid line number");	// pre-condition
	return mUnformatted[index];
}

void ChatBuffer::Step(float deltaMs)
{
	for (ChatLine& line : mUnformatted)
		line.age += deltaMs;
}

void ChatBuffer::DeleteOldest(unsigned int count)
{
	bool atBottom = (mScroll == GetBottomScrollPosition());

	unsigned int delUnformatted = 0;
	unsigned int delFormatted = 0;

	while (count > 0 && delUnformatted < mUnformatted.size())
	{
		++delUnformatted;

		// keep mFormatted in sync
		if (delFormatted < mFormatted.size())
		{
			LogAssert(mFormatted[delFormatted].first, "invalid format");
			++delFormatted;
			while (delFormatted < mFormatted.size() &&
					!mFormatted[delFormatted].first)
				++delFormatted;
		}

		--count;
	}

	mUnformatted.erase(mUnformatted.begin(), mUnformatted.begin() + delUnformatted);
	mFormatted.erase(mFormatted.begin(), mFormatted.begin() + delFormatted);

	if (atBottom)
		mScroll = GetBottomScrollPosition();
	else
		ScrollAbsolute(mScroll - delFormatted);
}

void ChatBuffer::DeleteByAge(float maxAge)
{
	unsigned int count = 0;
	while (count < mUnformatted.size() && mUnformatted[count].age > maxAge)
		++count;
	DeleteOldest(count);
}

unsigned int ChatBuffer::GetRows() const
{
	return mRows;
}

void ChatBuffer::ScrollTop()
{
	mScroll = GetTopScrollPosition();
}

void ChatBuffer::Reformat(unsigned int cols, unsigned int rows)
{
	if (cols == 0 || rows == 0)
	{
		// Clear formatted buffer
		mCols = 0;
		mRows = 0;
		mScroll = 0;
		mFormatted.clear();
	}
	else if (cols != mCols || rows != mRows)
	{
		// TODO: Avoid reformatting ALL lines (even invisible ones)
		// each time the console size changes.

		// Find out the scroll position in *unformatted* lines
		unsigned int restoreScrollUnformatted = 0;
		unsigned int restoreScrollFormatted = 0;
		bool atBottom = (mScroll == GetBottomScrollPosition());
		if (!atBottom)
		{
			for (int i = 0; i < mScroll; ++i)
			{
				if (mFormatted[i].first)
					++restoreScrollUnformatted;
			}
		}

		// If number of columns change, reformat everything
		if (cols != mCols)
		{
			mFormatted.clear();
			for (unsigned int i = 0; i < mUnformatted.size(); ++i)
			{
				if (i == restoreScrollUnformatted)
					restoreScrollFormatted = (unsigned int)mFormatted.size();
				FormatChatLine(mUnformatted[i], cols, mFormatted);
			}
		}

		// Update the console size
		mCols = cols;
		mRows = rows;

		// Restore the scroll position
		if (atBottom)
			ScrollBottom();
		else
			ScrollAbsolute(restoreScrollFormatted);
	}
}

const ChatFormattedLine& ChatBuffer::GetFormattedLine(unsigned int row) const
{
	int index = mScroll + (int) row;
	if (index >= 0 && index < (int) mFormatted.size())
		return mFormatted[index];

	return mEmptyFormattedLine;
}

void ChatBuffer::Scroll(int rows)
{
	ScrollAbsolute(mScroll + rows);
}

void ChatBuffer::ScrollAbsolute(int scroll)
{
	int top = GetTopScrollPosition();
	int bottom = GetBottomScrollPosition();

	mScroll = scroll;
	if (mScroll < top)
		mScroll = top;
	if (mScroll > bottom)
		mScroll = bottom;
}

void ChatBuffer::ScrollBottom()
{
	mScroll = GetBottomScrollPosition();
}

unsigned int ChatBuffer::FormatChatLine(const ChatLine& line, unsigned int cols,
		std::vector<ChatFormattedLine>& destination) const
{
	unsigned int numAdded = 0;
	std::vector<ChatFormattedFragment> nextFrags;
	ChatFormattedLine nextLine;
	ChatFormattedFragment tempFrag;
	unsigned int outColumn = 0;
	unsigned int inPos = 0;
	unsigned int hangingIndentation = 0;

	// Format the sender name and produce fragments
	if (!line.name.Empty()) 
    {
		tempFrag.text = L"<";
		tempFrag.column = 0;
		//tempFrag.bold = 0;
		nextFrags.push_back(tempFrag);
		tempFrag.text = line.name;
		tempFrag.column = 0;
		//tempFrag.bold = 1;
		nextFrags.push_back(tempFrag);
		tempFrag.text = L"> ";
		tempFrag.column = 0;
		//tempFrag.bold = 0;
		nextFrags.push_back(tempFrag);
	}

	std::wstring nameSanitized = line.name.C_Str();

	// Choose an indentation level
	if (line.name.Empty()) 
    {
		// Server messages
		hangingIndentation = 0;
	} 
    else if (nameSanitized.size() + 3 <= cols/2) 
    {
		// Names shorter than about half the console width
		hangingIndentation = (unsigned int)line.name.Size() + 3;
	} 
    else 
    {
		// Very long names
		hangingIndentation = 2;
	}
	//EnrichedString line_text(line.text);

	nextLine.first = true;
	bool textProcessing = false;

	// Produce fragments and layout them into lines
	while (!nextFrags.empty() || inPos < line.text.Size())
	{
		// Layout fragments into lines
		while (!nextFrags.empty())
		{
			ChatFormattedFragment& frag = nextFrags[0];
			if (frag.text.Size() <= cols - outColumn)
			{
				// Fragment fits into current line
				frag.column = outColumn;
				nextLine.fragments.push_back(frag);
				outColumn += (unsigned int)frag.text.Size();
				nextFrags.erase(nextFrags.begin());
			}
			else
			{
				// Fragment does not fit into current line
				// So split it up
				tempFrag.text = frag.text.Substr(0, cols - outColumn);
				tempFrag.column = outColumn;
				//tempFrag.bold = frag.bold;
				nextLine.fragments.push_back(tempFrag);
				frag.text = frag.text.Substr(cols - outColumn);
				outColumn = cols;
			}
			if (outColumn == cols || textProcessing)
			{
				// End the current line
				destination.push_back(nextLine);
				numAdded++;
				nextLine.fragments.clear();
				nextLine.first = false;

				outColumn = textProcessing ? hangingIndentation : 0;
			}
		}

		// Produce fragment
		if (inPos < line.text.Size())
		{
			unsigned int remainingInInput = (unsigned int)line.text.Size() - inPos;
			unsigned int remainingInOutput = cols - outColumn;

			// Determine a fragment length <= the minimum of
			// remaining_in_{in,out}put. Try to end the fragment
			// on a word boundary.
			unsigned int fragLength = 1, spacePos = 0;
			while (fragLength < remainingInInput &&
					fragLength < remainingInOutput)
			{
				if (iswspace(line.text.GetString()[inPos + fragLength]))
					spacePos = fragLength;
				++fragLength;
			}
			if (spacePos != 0 && fragLength < remainingInInput)
				fragLength = spacePos + 1;

			tempFrag.text = line.text.Substr(inPos, fragLength);
			tempFrag.column = 0;
			//tempFrag.bold = 0;
			nextFrags.push_back(tempFrag);
			inPos += fragLength;
			textProcessing = true;
		}
	}

	// End the last line
	if (numAdded == 0 || !nextLine.fragments.empty())
	{
		destination.push_back(nextLine);
		numAdded++;
	}

	return numAdded;
}

int ChatBuffer::GetTopScrollPosition() const
{
	int formattedCount = (int) mFormatted.size();
	int rows = (int) mRows;
	if (rows == 0)
		return 0;

	if (formattedCount <= rows)
		return formattedCount - rows;

	return 0;
}

int ChatBuffer::GetBottomScrollPosition() const
{
	int formattedCount = (int) mFormatted.size();
	int rows = (int) mRows;
	if (rows == 0)
		return 0;

	return formattedCount - rows;
}

void ChatBuffer::Resize(unsigned int scrollback)
{
	mScrollback = scrollback;
	if (mUnformatted.size() > mScrollback)
		DeleteOldest((unsigned int)mUnformatted.size() - mScrollback);
}


ChatPrompt::ChatPrompt(const std::wstring& prompt, unsigned int historyLimit) :
	mPrompt(prompt), mHistoryLimit(historyLimit)
{
}

void ChatPrompt::Input(wchar_t ch)
{
	mLine.insert(mCursor, 1, ch);
	mCursor++;
	ClampView();
	mNickCompletionStart = 0;
	mNickCompletionEnd = 0;
}

void ChatPrompt::Input(const std::wstring& str)
{
	mLine.insert(mCursor, str);
	mCursor += (unsigned int)str.size();
	ClampView();
	mNickCompletionStart = 0;
	mNickCompletionEnd = 0;
}

void ChatPrompt::AddToHistory(const std::wstring& line)
{
	if (!line.empty() && (mHistory.size() == 0 || mHistory.back() != line)) 
    {
		// Remove all duplicates
		mHistory.erase(std::remove(mHistory.begin(), mHistory.end(), line), mHistory.end());
		// Push unique line
		mHistory.push_back(line);
	}
	if (mHistory.size() > mHistoryLimit)
		mHistory.erase(mHistory.begin());
	mHistoryIndex = (unsigned int)mHistory.size();
}

void ChatPrompt::Clear()
{
	mLine.clear();
	mView = 0;
	mCursor = 0;
	mNickCompletionStart = 0;
	mNickCompletionEnd = 0;
}

std::wstring ChatPrompt::Replace(const std::wstring& line)
{
	std::wstring oldLine = mLine;
	mLine =  line;
	mView = mCursor = (int)line.size();
	ClampView();
	mNickCompletionStart = 0;
	mNickCompletionEnd = 0;
	return oldLine;
}

void ChatPrompt::HistoryPrevious()
{
	if (mHistoryIndex != 0)
	{
		--mHistoryIndex;
		Replace(mHistory[mHistoryIndex]);
	}
}

void ChatPrompt::HistoryNext()
{
	if (mHistoryIndex + 1 >= mHistory.size())
	{
		mHistoryIndex = (unsigned int)mHistory.size();
		Replace(L"");
	}
	else
	{
		++mHistoryIndex;
		Replace(mHistory[mHistoryIndex]);
	}
}

void ChatPrompt::NickCompletion(const std::list<std::string>& names, bool backwards)
{
	// Two cases:
	// (a) mNickCompletionStart == mNickCompletionEnd == 0
	//     Then no previous nick completion is active.
	//     Get the word around the cursor and replace with any nick
	//     that has that word as a prefix.
	// (b) else, continue a previous nick completion.
	//     mNickCompletionStart..mNickCompletionEnd are the
	//     interval where the originally used prefix was. Cycle
	//     through the list of completions of that prefix.
	unsigned int prefixStart = mNickCompletionStart;
	unsigned int prefixEnd = mNickCompletionEnd;
	bool initial = (prefixEnd == 0);
	if (initial)
	{
		// no previous nick completion is active
		prefixStart = prefixEnd = mCursor;
		while (prefixStart > 0 && !iswspace(mLine[prefixStart-1]))
			--prefixStart;
		while (prefixEnd < mLine.size() && !iswspace(mLine[prefixEnd]))
			++prefixEnd;
		if (prefixStart == prefixEnd)
			return;
	}
	std::wstring prefix = mLine.substr(prefixStart, prefixEnd - prefixStart);

	// find all names that start with the selected prefix
	std::vector<std::wstring> completions;
	for (std::string const& name : names)
    {
        std::wstring completion = ToWideString(name);
		if (StringStartsWith(completion, prefix, true)) 
        {
			if (prefixStart == 0)
				completion += L": ";
			completions.push_back(completion);
		}
	}

	if (completions.empty())
		return;

	// find a replacement string and the word that will be replaced
	unsigned int wordEnd = prefixEnd;
	unsigned int replacementIndex = 0;
	if (!initial)
	{
		while (wordEnd < mLine.size() && !iswspace(mLine[wordEnd]))
			++wordEnd;
		std::wstring word = mLine.substr(prefixStart, wordEnd - prefixStart);

		// cycle through completions
		for (unsigned int i = 0; i < completions.size(); ++i)
		{
			if (StringEqual(word, completions[i], true))
			{
				if (backwards)
					replacementIndex = i + (unsigned int)completions.size() - 1;
				else
					replacementIndex = i + 1;
				replacementIndex %= completions.size();
				break;
			}
		}
	}
	std::wstring replacement = completions[replacementIndex];
	if (wordEnd < mLine.size() && iswspace(mLine[wordEnd]))
		++wordEnd;

	// replace existing word with replacement word,
	// place the cursor at the end and record the completion prefix
	mLine.replace(prefixStart, wordEnd - prefixStart, replacement);
	mCursor = prefixStart + (unsigned int)replacement.size();
	ClampView();
	mNickCompletionStart = prefixStart;
	mNickCompletionEnd = prefixEnd;
}

void ChatPrompt::Reformat(unsigned int cols)
{
	if (cols <= mPrompt.size())
	{
		mCols = 0;
		mView = mCursor;
	}
	else
	{
		int length = (unsigned int)mLine.size();
		bool wasAtEnd = (mView + mCols >= length + 1);
		mCols = cols - (unsigned int)mPrompt.size();
		if (wasAtEnd)
			mView = length;
		ClampView();
	}
}

std::wstring ChatPrompt::GetVisiblePortion() const
{
	return mPrompt + mLine.substr(mView, mCols);
}

int ChatPrompt::GetVisibleCursorPosition() const
{
	return mCursor - mView + (unsigned int)mPrompt.size();
}

void ChatPrompt::CursorOperation(CursorOp op, CursorOpDir dir, CursorOpScope scope)
{
	int oldCursor = mCursor;
	int newCursor = mCursor;

	int length = (int)mLine.size();
	int increment = (dir == CURSOROP_DIR_RIGHT) ? 1 : -1;

	switch (scope) 
    {
	    case CURSOROP_SCOPE_CHARACTER:
		    newCursor += increment;
		    break;
	    case CURSOROP_SCOPE_WORD:
		    if (dir == CURSOROP_DIR_RIGHT) 
            {
			    // skip one word to the right
			    while (newCursor < length && iswspace(mLine[newCursor]))
				    newCursor++;
			    while (newCursor < length && !iswspace(mLine[newCursor]))
				    newCursor++;
			    while (newCursor < length && iswspace(mLine[newCursor]))
				    newCursor++;
		    } else 
            {
			    // skip one word to the left
			    while (newCursor >= 1 && iswspace(mLine[newCursor - 1]))
				    newCursor--;
			    while (newCursor >= 1 && !iswspace(mLine[newCursor - 1]))
				    newCursor--;
		    }
		    break;
	    case CURSOROP_SCOPE_LINE:
		    newCursor += increment * length;
		    break;
	    case CURSOROP_SCOPE_SELECTION:
		    break;
	}

	newCursor = std::max(std::min(newCursor, length), 0);

	switch (op) 
    {
	    case CURSOROP_MOVE:
		    mCursor = newCursor;
		    mCursorLen = 0;
		    break;
	    case CURSOROP_DELETE:
		    if (mCursorLen > 0) 
            { 
                // Delete selected text first
			    mLine.erase(mCursor, mCursorLen);
		    } 
            else 
            {
			    mCursor = std::min(newCursor, oldCursor);
			    mLine.erase(mCursor, abs(newCursor - oldCursor));
		    }
		    mCursorLen = 0;
		    break;
	    case CURSOROP_SELECT:
		    if (scope == CURSOROP_SCOPE_LINE) 
            {
			    mCursor = 0;
			    mCursorLen = length;
		    } 
            else 
            {
			    mCursor = std::min(newCursor, oldCursor);
			    mCursorLen += abs(newCursor - oldCursor);
			    mCursorLen = std::min(mCursorLen, length - mCursor);
		    }
		    break;
	}

	ClampView();

	mNickCompletionStart = 0;
	mNickCompletionEnd = 0;
}

void ChatPrompt::ClampView()
{
	int length = (int)mLine.size();
	if (length + 1 <= mCols)
	{
		mView = 0;
	}
	else
	{
        mView = std::min(mView, length + 1 - mCols);
        mView = std::min(mView, mCursor);
        mView = std::max(mView, mCursor - mCols + 1);
        mView = std::max(mView, 0);
	}
}

ChatBackend::ChatBackend():
	mConsoleBuffer(500),
	mRecentBuffer(6),
	mPrompt(L"]", 500)
{

}

void ChatBackend::AddMessage(const std::wstring& name, std::wstring text)
{
	// Note: A message may consist of multiple lines, for example the MOTD.
	//text = TranslateString(text);
	WStrfnd fnd(text);
	while (!fnd.AtEnd())
	{
		std::wstring line = fnd.Next(L"\n");
		mConsoleBuffer.AddLine(name, line);
		mRecentBuffer.AddLine(name, line);
	}
}

void ChatBackend::AddUnparsedMessage(std::wstring message)
{
	// TODO: Remove the need to parse chat messages client-side, by sending
	// separate name and text fields in TOCLIENT_CHAT_MESSAGE.

	if (message.size() >= 2 && message[0] == L'<')
	{
		size_t closing = message.find_first_of(L'>', 1);
		if (closing != std::wstring::npos &&
            closing + 2 <= message.size() &&
            message[closing+1] == L' ')
		{
			std::wstring name = message.substr(1, closing - 1);
			std::wstring text = message.substr(closing + 2);
			AddMessage(name, text);
			return;
		}
	}

	// Unable to parse, probably a server message.
	AddMessage(L"", message);
}

ChatBuffer& ChatBackend::GetConsoleBuffer()
{
	return mConsoleBuffer;
}

ChatBuffer& ChatBackend::GetRecentBuffer()
{
	return mRecentBuffer;
}

EnrichedString ChatBackend::GetRecentChat() const
{
	EnrichedString result;
	for (unsigned int i = 0; i < mRecentBuffer.GetLineCount(); ++i) 
    {
		const ChatLine& line = mRecentBuffer.GetLine(i);
		if (i != 0)
			result += L"\n";
		if (!line.name.Empty()) 
        {
			result += L"<";
			result += line.name;
			result += L"> ";
		}
		result += line.text;
	}
	return result;
}

ChatPrompt& ChatBackend::GetPrompt()
{
	return mPrompt;
}

void ChatBackend::Reformat(unsigned int cols, unsigned int rows)
{
	mConsoleBuffer.Reformat(cols, rows);

	// no need to reformat mRecentBuffer, its formatted lines
	// are not used
	mPrompt.Reformat(cols);
}

void ChatBackend::ClearRecentChat()
{
	mRecentBuffer.Clear();
}


void ChatBackend::ApplySettings()
{
    int recentLines = Settings::Get()->GetInt("recent_chat_messages");
    recentLines = std::clamp(recentLines, 2, 20);
	mRecentBuffer.Resize(recentLines);
}

void ChatBackend::Step(float deltaMs)
{
	mRecentBuffer.Step(deltaMs);
	mRecentBuffer.DeleteByAge(60.0);

	// no need to age messages in anything but mRecentBuffer
}

void ChatBackend::Scroll(int rows)
{
	mConsoleBuffer.Scroll(rows);
}

void ChatBackend::ScrollPageDown()
{
	mConsoleBuffer.Scroll(mConsoleBuffer.GetRows());
}

void ChatBackend::ScrollPageUp()
{
	mConsoleBuffer.Scroll(-(int)mConsoleBuffer.GetRows());
}
