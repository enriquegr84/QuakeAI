// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef UICHECKBOX_H
#define UICHECKBOX_H

#include "UIElement.h"

#include "Graphic/State/BlendState.h"
#include "Graphic/Scene/Hierarchy/Visual.h"

class BaseUICheckBox : public BaseUIElement
{
public:

	//! constructor
	BaseUICheckBox(BaseUI* ui, int id, RectangleShape<2, int> rectangle)
		: BaseUIElement(UIET_CHECK_BOX, id, rectangle) {}

	//! Initialize checkbox
	virtual void OnInit() = 0;

	//! Set if box is checked.
	virtual void SetChecked(bool checked) = 0;

	//! Returns true if box is checked.
	virtual bool IsChecked() const = 0;
};

class UICheckBox : public BaseUICheckBox
{
public:

	//! constructor
	UICheckBox(BaseUI* ui, int id, RectangleShape<2, int> rectangle, bool checked);

	//! initialize checkbox
	virtual void OnInit();

	//! set if box is checked
	virtual void SetChecked(bool checked);

	//! returns if box is checked
	virtual bool IsChecked() const;

	//! called if an event happened.
	virtual bool OnEvent(const Event& evt);

	//! draws the element and its children
	virtual void Draw();

private:

	BaseUI* mUI;

	std::shared_ptr<Visual> mVisual;
	std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<BlendState> mBlendState;

	unsigned int mCheckTime;
	bool mPressed;
	bool mChecked;
};

#endif
