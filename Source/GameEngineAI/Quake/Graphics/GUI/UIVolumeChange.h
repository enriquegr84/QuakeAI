#ifndef UIVOLUMECHANGE_H
#define UIVOLUMECHANGE_H

#include "Graphic/UI/Element/UIForm.h"

class UIVolumeChange : public BaseUIForm
{
public:
    UIVolumeChange(BaseUI* ui, int id, RectangleShape<2, int> rectangle);
	~UIVolumeChange();

	virtual void RemoveChildren(bool cascade);

	/*
		Remove and re-add (or reposition) stuff
	*/
	virtual void RegenerateUI(Vector2<unsigned int> screenSize);
	virtual void DrawForm();

	virtual bool OnEvent(const Event& evt);
	virtual bool PausesGame() { return true; }

protected:
	
    virtual std::wstring GetLabel(int id) { return L""; }
    virtual std::string GetName(int id) { return ""; }

private:

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;
	std::shared_ptr<BlendState> mBlendState;
};

#endif