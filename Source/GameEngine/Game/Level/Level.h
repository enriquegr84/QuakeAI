//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef LEVEL_H
#define LEVEL_H

#include "GameEngineStd.h"


/**
  * \ingroup levels
  */
class Level
{

public:

	Level(const std::wstring& filePath);
	~Level();
    void Update(float dt);
    void Reset();

    // ------------------------------------------------------------------------
    /** Returns a unique id for this level (the directory name). */
    const std::wstring& GetId() const {return mId;}
	// ------------------------------------------------------------------------
	/** Returns the name for this level */
	const std::wstring& GetName() const { return mName; }
    // ------------------------------------------------------------------------
    /** Returns the author for this level */
    const std::wstring& GetAuthor() const { return mAuthor; }
    // ------------------------------------------------------------------------
    /** Returns the description for this level. */
    const std::wstring& GetDescription() const { return mDescription; }
	// ------------------------------------------------------------------------
    /** Returns the filename of this level. */
    const std::wstring& GetFilePath() const { return mFilePath; }
    // ------------------------------------------------------------------------
    /** Returns the iconpath of this level. */
    const std::wstring& GetIconPath() const { return mIconPath; }
    // ------------------------------------------------------------------------
    /** Returns the path of level assets */
    const std::wstring& GetPath() const { return mPath; }

protected:

	/** A simple class to keep information about a level mode. */
	class LevelMode
	{
	public:
		std::wstring mName; /* Name / description of this mode. */
		std::wstring mScene; /* Name of the scene file to use.   */

        /** Default constructor, sets default names for all fields. */
		LevelMode()
			: mName(L"default"), mScene(L"default")
		{

		}

		~LevelMode()
		{

		}

	};   // LevelMode


	std::wstring mId;

    /** The full path of the level assets. */
    std::wstring mPath;

    std::wstring mIconPath;

    /** Author of the level*/
    std::wstring mAuthor;

    /** Name of the level to display. */
	std::wstring mName;

    /** Description of the level to display. */
    std::wstring mDescription;

    /** The full filename of the config (xml) file. */
    std::wstring mFilePath;

	void LoadLevelInfo();
	bool LoadMainLevel(const tinyxml2::XMLElement* pNode);

};   // class Level

#endif