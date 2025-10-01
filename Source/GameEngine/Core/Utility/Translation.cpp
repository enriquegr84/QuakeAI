/*
Minetest
Copyright (C) 2017 Nore, Nathanaël Courant <nore@mesecons.net>

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

#include "Translation.h"
#include "Core/Logger/Logger.h"
#include "Core/Utility/StringUtil.h"


void Translations::Clear()
{
	mTranslations.clear();
}

const std::wstring& Translations::GetTranslation(
		const std::wstring& textdomain, const std::wstring& str)
{
	std::wstring key = textdomain + L"|" + str;
	try 
    {
		return mTranslations.at(key);
	} 
    catch (const std::out_of_range &) 
    {
		LogInformation("Translations: can't find translation for string \"" +
            ToString(str) + "\" in textdomain \"" + ToString(textdomain) + "\"");
		// Silence that warning in the future
		mTranslations[key] = str;
		return str;
	}
}

void Translations::LoadTranslation(const std::string& data)
{
    std::string line;
	std::istringstream is(data);
	std::wstring textdomain;

	while (is.good()) 
    {
		std::getline(is, line);
		// Trim last character if file was using a \r\n line ending
		if (line.length () > 0 && line[line.length() - 1] == '\r')
			line.resize(line.length() - 1);

		if (StringStartsWith(line, "# textdomain:")) 
        {
			textdomain = ToWideString(Trim(StringSplit(line, ':')[1]));
		}
		if (line.empty() || line[0] == '#')
			continue;

		std::wstring wline = ToWideString(line);
		if (wline.empty())
			continue;

		// Read line
		// '=' marks the key-value pair, but may be escaped by an '@'.
		// '\n' may also be escaped by '@'.
		// All other escapes are preserved.

		size_t i = 0;
		std::wostringstream word1, word2;
		while (i < wline.length() && wline[i] != L'=') 
        {
			if (wline[i] == L'@') 
            {
				if (i + 1 < wline.length()) 
                {
					if (wline[i + 1] == L'=') 
                    {
						word1.put(L'=');
					} 
                    else if (wline[i + 1] == L'n') 
                    {
						word1.put(L'\n');
					} 
                    else 
                    {
						word1.put(L'@');
						word1.put(wline[i + 1]);
					}
					i += 2;
				}
                else
                {
					// End of line, go to the next one.
					word1.put(L'\n');
					if (!is.good())
						break;

					i = 0;
					std::getline(is, line);
					wline = ToWideString(line);
				}
			} 
            else 
            {
				word1.put(wline[i]);
				i++;
			}
		}

		if (i == wline.length()) 
        {
            LogError("Malformed translation line \"" + line + "\"");
			continue;
		}
		i++;

		while (i < wline.length()) 
        {
			if (wline[i] == L'@') 
            {
				if (i + 1 < wline.length())
                {
					if (wline[i + 1] == L'=')
                    {
						word2.put(L'=');
					} 
                    else if (wline[i + 1] == L'n') 
                    {
						word2.put(L'\n');
					}
                    else 
                    {
						word2.put(L'@');
						word2.put(wline[i + 1]);
					}
					i += 2;
				} 
                else
                {
					// End of line, go to the next one.
					word2.put(L'\n');
					if (!is.good()) 
						break;

					i = 0;
					std::getline(is, line);
					wline = ToWideString(line);
				}
			} 
            else 
            {
				word2.put(wline[i]);
				i++;
			}
		}

		std::wstring oword1 = word1.str(), oword2 = word2.str();
		if (!oword2.empty()) 
        {
			std::wstring translationIndex = textdomain + L"|";
            translationIndex.append(oword1);
			mTranslations[translationIndex] = oword2;
		}
        else 
        {
			LogInformation("Ignoring empty translation for \"" + ToString(oword1) + "\"");
		}
	}
}
