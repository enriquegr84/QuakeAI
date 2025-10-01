/*
Part of Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "UIPasswordChange.h"

#include "Graphic/UI/UIEngine.h"

#include "Application/Settings.h"

const int IDOldPassword = 256;
const int IDNewPassword1 = 257;
const int IDNewPassword2 = 258;
const int IDChange = 259;
const int IDMessage = 260;
const int IDCancel = 261;

UIPasswordChange::UIPasswordChange(BaseUI* ui, int id, RectangleShape<2, int> rectangle) 
    : BaseUIForm(ui, id, rectangle)
{
    // Create a vertex buffer for a single triangle.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

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

    mEffect = std::make_shared<ColorEffect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

UIPasswordChange::~UIPasswordChange()
{
	RemoveChildren(true);
}

void UIPasswordChange::RegenerateUI(Vector2<unsigned int> screenSize)
{
	/*
		save current input
	*/
	AcceptInput();

	/*
		Remove stuff
	*/
	RemoveChildren(true);

	/*
		Calculate new sizes and positions
	*/
    const float s = Settings::Get()->GetFloat("gui_scaling");
    mDesiredRect = RectangleShape<2, int>();
    mDesiredRect.mExtent = Vector2<int>{ (int)(580 * s), (int)(300 * s) };
    mDesiredRect.mCenter = Vector2<int>{ (int)(screenSize[0] / 2), (int)(screenSize[1] / 2) };
    RecalculateAbsolutePosition(false);

    Vector2<int> topLeft{ (int)(40 * s), 0 };

	/*
		Add stuff
	*/
	int yPos = (int)(50 * s);
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(150 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(25 * s), (int)(yPos + 6 * s)};

        const std::wstring text = L"Old Password";
        mUI->AddStaticText(text.c_str(), rect, false, true, shared_from_this(), -1);
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(230 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(160 * s), yPos};

		std::shared_ptr<BaseUIEditBox> editBox = mUI->AddEditBox(
				mOldPass.c_str(), rect, true, true, shared_from_this(), IDOldPassword);
		mUI->SetFocus(editBox);
        editBox->SetPasswordBox(true);
	}
    yPos += (int)(50 * s);
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(150 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(25 * s), (int)(yPos + 6 * s)};

        const std::wstring text = L"New Password";
        mUI->AddStaticText(text.c_str(), rect, false, true, shared_from_this(), -1);
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(230 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(160 * s), yPos};

        std::shared_ptr<BaseUIEditBox> editBox = mUI->AddEditBox(
            mNewPass.c_str(), rect, true, true, shared_from_this(), IDNewPassword1);
        editBox->SetPasswordBox(true);
	}
	yPos += (int)(50 * s);
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(150 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(25 * s), (int)(yPos + 6 * s)};

        const std::wstring text = L"Confirm Password";
        mUI->AddStaticText(text.c_str(), rect, false, true, shared_from_this(), -1);
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(230 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(160 * s), yPos};

        std::shared_ptr<BaseUIEditBox> editBox = mUI->AddEditBox(
            mNewPassConfirm.c_str(), rect, true, true, shared_from_this(), IDNewPassword2);
        editBox->SetPasswordBox(true);
	}

	yPos += (int)(50 * s);
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(100 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 4 + 56 * s), yPos};

        std::wstring text = L"Change";
        mUI->AddButton(rect, shared_from_this(), IDChange, text.c_str());
	}
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(100 * s), (int)(30 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{
            (int)(mDesiredRect.mExtent[0] / 4 + 185 * s), yPos};

        std::wstring text = L"Cancel";
        mUI->AddButton(rect, shared_from_this(), IDCancel, text.c_str());
	}

	yPos += (int)(50 * s);
	{
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)(300 * s), (int)(20 * s) };
        rect.mCenter = rect.mExtent / 2;
        rect.mExtent += topLeft + Vector2<int>{(int)(35 * s), yPos};

        const std::wstring text = L"Passwords do not match!";
        std::shared_ptr<BaseUIStaticText> staticText = 
            mUI->AddStaticText(text.c_str(), rect, false, true, shared_from_this(), IDMessage);
        staticText->SetVisible(false);
	}
}

void UIPasswordChange::DrawForm()
{
    std::shared_ptr<BaseUISkin> skin = mUI->GetSkin();
    if (!skin)
        return;

    SColor bgColor(140, 0, 0, 0);
    skin->Draw2DRectangle(bgColor, mVisual, mAbsoluteRect, &mAbsoluteClippingRect);
    BaseUIElement::Draw();
}

void UIPasswordChange::AcceptInput()
{
    std::shared_ptr<BaseUIElement> el;
    el = GetElementFromId(IDOldPassword);
	if (el != NULL)
		mOldPass = el->GetText();
    el = GetElementFromId(IDNewPassword1);
	if (el != NULL)
		mNewPass = el->GetText();
    el = GetElementFromId(IDNewPassword2);
	if (el != NULL)
		mNewPassConfirm = el->GetText();
}

bool UIPasswordChange::ProcessInput()
{
	if (mNewPass != mNewPassConfirm) 
    {
        std::shared_ptr<BaseUIElement> el = GetElementFromId(IDMessage);
		if (el != NULL)
            el->SetVisible(true);
		return false;
	}
	//mVisualEnv->SendChangePassword(ToString(mOldPass), ToString(mNewPass));
	return true;
}

bool UIPasswordChange::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_KEY_INPUT_EVENT)
    {
		// clang-format off
		if ((evt.mKeyInput.mKey == KEY_ESCAPE ||
            evt.mKeyInput.mKey == KEY_CANCEL) &&
            evt.mKeyInput.mPressedDown)
        {
			QuitForm();
			return true;
		}
		// clang-format on
		if (evt.mKeyInput.mKey == KEY_RETURN && evt.mKeyInput.mPressedDown)
        {
			AcceptInput();
			if (ProcessInput())
                QuitForm();
			return true;
		}
	}
	if (evt.mEventType == ET_UI_EVENT)
    {
		if (evt.mUIEvent.mEventType == UIEVT_ELEMENT_FOCUS_LOST && IsVisible())
        {
			if (!CanTakeFocus(evt.mUIEvent.mElement))
            {
				LogInformation("UIPasswordChange: Not allowing focus change.");
				// Returning true disables focus change
				return true;
			}
		}
		if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
        {
			switch (evt.mUIEvent.mCaller->GetID())
            {
			    case IDChange:
				    AcceptInput();
				    if (ProcessInput())
                        QuitForm();
				    return true;
			    case IDCancel:
                    QuitForm();
				    return true;
			}
		}
		if (evt.mUIEvent.mEventType == UIEVT_EDITBOX_ENTER)
        {
			switch (evt.mUIEvent.mCaller->GetID())
            {
			    case IDOldPassword:
			    case IDNewPassword1:
			    case IDNewPassword2:
				    AcceptInput();
				    if (ProcessInput())
                        QuitForm();
				    return true;
			}
		}
	}

	return mParent ? mParent->OnEvent(evt) : false;
}

std::string UIPasswordChange::GetName(int id)
{
	switch (id) 
    {
	    case IDOldPassword:
		    return "old_password";
	    case IDNewPassword1:
		    return "new_password_1";
	    case IDNewPassword2:
		    return "new_password_2";
	}
	return "";
}
