/*
 Minetest
 Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
 Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>
 Copyright (C) 2013 teddydestodes <derkomtur@schattengang.net>

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

#ifndef UIKEYCHANGE_H
#define UIKEYCHANGE_H

#include "Application/System/KeyEvent.h"

#include "Graphic/UI/Element/UIForm.h"
#include "Graphic/UI/Element/UIButton.h"

struct KeySetting
{
	int id;
    KeyAction key;
    std::string settingName;
	std::wstring buttonName;
	std::shared_ptr<BaseUIButton> button;
};

class UIKeyChange : public BaseUIForm
{
public:
    UIKeyChange(BaseUI* ui, int id, RectangleShape<2, int> rectangle);
	~UIKeyChange();

    virtual void RemoveChildren(bool cascade);

	// Remove and re-add (or reposition) stuff
	virtual void RegenerateUI(Vector2<unsigned int> screenSize);
	virtual void DrawForm();

	virtual bool OnEvent(const Event& evt);
	virtual bool PausesGame() { return true; }

    bool AcceptInput();

protected:

	virtual std::wstring GetLabel(int id) { return L""; }
    virtual std::string GetName(int id) { return ""; }

private:

	void InitKeys();
	bool ResetForm();

	void AddKey(int id, const wchar_t* buttonName, const std::string& settingName);

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;
	std::shared_ptr<BlendState> mBlendState;

	bool mShiftDown = false;
    KeySetting* mActiveKey = nullptr;
    std::vector<KeySetting*> mKeySettings;
	std::shared_ptr<BaseUIStaticText> mKeyUsedText = nullptr;
};

#endif