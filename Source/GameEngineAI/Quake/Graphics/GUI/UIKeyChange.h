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
	/*
	 Remove and re-add (or reposition) stuff
	 */
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