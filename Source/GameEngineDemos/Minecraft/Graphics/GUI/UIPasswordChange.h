/*
Part of Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef UIPASSWORDCHANGE_H
#define UIPASSWORDCHANGE_H

#include "Application/System/KeyEvent.h"

#include "Graphic/UI/Element/UIForm.h"
#include "Graphic/UI/Element/UIButton.h"

class UIPasswordChange : public BaseUIForm
{
public:
    UIPasswordChange(BaseUI* ui, int id, RectangleShape<2, int> rectangle);
    ~UIPasswordChange();

    // Remove and re-add (or reposition) stuff
    virtual void RegenerateUI(Vector2<unsigned int> screenSize);
    virtual void DrawForm();

    virtual bool OnEvent(const Event& evt);

	void AcceptInput();
	bool ProcessInput();

protected:

    virtual std::wstring GetLabel(int id) { return L""; }
    virtual std::string GetName(int id);

private:

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;

	std::wstring mOldPass = L"";
    std::wstring mNewPass = L"";
    std::wstring mNewPassConfirm = L"";
};

#endif