//========================================================================
// DemosView.cpp : source file for the sample game
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/GameEngine/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Image/ImageResource.h"

#include "Graphic/Scene/Scene.h"
/*
#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Hierarchy/Camera.h"
*/
#include "Core/OS/OS.h"
#include "Core/Event/Event.h"

#include "Physic/PhysicEventListener.h"

#include "Game/View/HumanView.h"

#include "Game/Actor/Actor.h"
#include "Game/Actor/RenderComponent.h"

#include "AreaLightsApp.h"
#include "AreaLightsView.h"

//========================================================================
//
// AreaLightsHUD implementation
//
//
//========================================================================

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define CID_CAMERA_WINDOW					(1)
#define CID_CAMERA_POSITION					(2)
#define CID_CAMERA_YAW						(3)
#define CID_CAMERA_PITCH					(4)
#define CID_CAMERA_ROLL						(5)

#define CID_LIGHT_WINDOW					(6)
#define CID_LIGHT_X							(7)
#define CID_LIGHT_Y							(8)
#define CID_LIGHT_Z							(9)
#define CID_LIGHT_INTENSITY					(10)
#define CID_LIGHT_DIFFUSE					(11)
#define CID_LIGHT_DIFFUSER					(12)
#define CID_LIGHT_DIFFUSEG					(13)
#define CID_LIGHT_DIFFUSEB					(14)
#define CID_SET_DIFFUSE_RADIO				(15)
#define CID_LIGHT_AMBIENT					(16)
#define CID_LIGHT_AMBIENTR					(17)
#define CID_LIGHT_AMBIENTG					(18)
#define CID_LIGHT_AMBIENTB					(19)
#define CID_SET_AMBIENT_RADIO				(20)
#define CID_LIGHT_CONSTANT					(21)
#define CID_LIGHT_LINEAR					(22)
#define CID_LIGHT_QUADRATIC					(23)


AreaLightsHUD::AreaLightsHUD()
{

}


AreaLightsHUD::~AreaLightsHUD() 
{ 

}

bool AreaLightsHUD::OnInit()
{
	BaseUI::OnInit();

	System* system = System::Get();
	system->GetCursorControl()->SetVisible(true);

	// set a nicer font
	const std::shared_ptr<BaseUIFont>& font = GetFont(L"DefaultFont");
	if (font) GetSkin()->SetFont(font);

    GetSkin()->SetColor(DC_BUTTON_TEXT, SColor(240, 170, 170, 170));
    GetSkin()->SetColor(DC_3D_HIGH_LIGHT, SColor(240, 34, 34, 34));
    GetSkin()->SetColor(DC_3D_FACE, SColor(240, 68, 68, 68));
    GetSkin()->SetColor(DC_EDITABLE, SColor(240, 68, 68, 68));
    GetSkin()->SetColor(DC_FOCUSED_EDITABLE, SColor(240, 84, 84, 84));
    GetSkin()->SetColor(DC_WINDOW, SColor(240, 102, 102, 102));

	//gui size
	Renderer* renderer = Renderer::Get();
	Vector2<unsigned int> screenSize(renderer->GetScreenSize());
	RectangleShape<2, int> screenRectangle;
	screenRectangle.mCenter[0] = screenSize[0] / 2;
	screenRectangle.mCenter[1] = screenSize[1] / 2;
	screenRectangle.mExtent[0] = (int)(screenSize[0] / 2);
	screenRectangle.mExtent[1] = (int)(screenSize[1] / 5.5f);
	std::shared_ptr<BaseUIWindow> cameraWindow = 
		AddWindow(screenRectangle, false, L"Camera", 0, CID_CAMERA_WINDOW);
	cameraWindow->GetCloseButton()->SetToolTipText(L"Camera");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 32;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> cameraPositionLine =
		AddStaticText(L"Position:", screenRectangle, false, false, cameraWindow, -1, false);
	cameraPositionLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 32;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> cameraPosition = 
		AddScrollBar(true, true, screenRectangle, cameraWindow, CID_CAMERA_POSITION);
	cameraPosition->SetMin(0);
	cameraPosition->SetMax(80);
	cameraPosition->SetSmallStep(1);
	cameraPosition->SetLargeStep(1);
	cameraPosition->SetPosition(0);
	cameraPosition->SetToolTipText(L"Set camera position");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 52;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> cameraYawLine =
		AddStaticText(L"Yaw:", screenRectangle, false, false, cameraWindow, -1, false);
	cameraYawLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 52;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> cameraYaw =
		AddScrollBar(true, true, screenRectangle, cameraWindow, CID_CAMERA_YAW);
	cameraYaw->SetMin(-180);
	cameraYaw->SetMax(180);
	cameraYaw->SetSmallStep(1);
	cameraYaw->SetLargeStep(1);
	cameraYaw->SetPosition(0);
	cameraYaw->SetToolTipText(L"Set camera yaw");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 72;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> cameraPitchLine =
		AddStaticText(L"Pitch:", screenRectangle, false, false, cameraWindow, -1, false);
	cameraPitchLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 72;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> cameraPitch =
		AddScrollBar(true, true, screenRectangle, cameraWindow, CID_CAMERA_PITCH);
	cameraPitch->SetMin(-89);
	cameraPitch->SetMax(89);
	cameraPitch->SetSmallStep(1);
	cameraPitch->SetLargeStep(1);
	cameraPitch->SetPosition(0);
	cameraPitch->SetToolTipText(L"Set camera pitch");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 92;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> cameraRollLine =
		AddStaticText(L"Roll:", screenRectangle, false, false, cameraWindow, -1, false);
	cameraRollLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 92;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> cameraRoll =
		AddScrollBar(true, true, screenRectangle, cameraWindow, CID_CAMERA_ROLL);
	cameraRoll->SetMin(-180);
	cameraRoll->SetMax(180);
	cameraRoll->SetSmallStep(1);
	cameraRoll->SetLargeStep(1);
	cameraRoll->SetPosition(0);
	cameraRoll->SetToolTipText(L"Set camera roll");

	cameraWindow->UpdateAbsoluteTransformation();

	screenRectangle.mCenter[0] = screenSize[0] / 2;
	screenRectangle.mCenter[1] = screenSize[1] / 2;
	screenRectangle.mExtent[0] = (int)(screenSize[0] / 2);
	screenRectangle.mExtent[1] = (int)(screenSize[1] / 3.f);
	std::shared_ptr<BaseUIWindow> lightWindow =
		AddWindow(screenRectangle, false, L"Light", 0, CID_LIGHT_WINDOW);
	lightWindow->GetCloseButton()->SetToolTipText(L"Light");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 32;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightPositionLine =
		AddStaticText(L"Position", screenRectangle, false, false, lightWindow, -1, false);
	lightPositionLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 52;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightXPositionLine =
		AddStaticText(L"X:", screenRectangle, false, false, lightWindow, -1, false);
	lightXPositionLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 52;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightXPosition =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_X);
	lightXPosition->SetMin(-40);
	lightXPosition->SetMax(40);
	lightXPosition->SetSmallStep(1);
	lightXPosition->SetLargeStep(1);
	lightXPosition->SetPosition(0);
	lightXPosition->SetToolTipText(L"Set X position");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 72;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightYPositionLine =
		AddStaticText(L"Y:", screenRectangle, false, false, lightWindow, -1, false);
	lightYPositionLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 72;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightYPosition =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_Y);
	lightYPosition->SetMin(-40);
	lightYPosition->SetMax(40);
	lightYPosition->SetSmallStep(1);
	lightYPosition->SetLargeStep(1);
	lightYPosition->SetPosition(4);
	lightYPosition->SetToolTipText(L"Set Y position");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 92;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightZPositionLine =
		AddStaticText(L"Z:", screenRectangle, false, false, lightWindow, -1, false);
	lightZPositionLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 92;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightZPosition =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_Z);
	lightZPosition->SetMin(-40);
	lightZPosition->SetMax(40);
	lightZPosition->SetSmallStep(1);
	lightZPosition->SetLargeStep(1);
	lightZPosition->SetPosition(0);
	lightZPosition->SetToolTipText(L"Set Z position");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 122;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightIntensityColorLine =
		AddStaticText(L"Intensity/Color", screenRectangle, false, false, lightWindow, -1, false);
	lightIntensityColorLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 142;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightIntensityLine =
		AddStaticText(L"Intensity:", screenRectangle, false, false, lightWindow, -1, false);
	lightIntensityLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 142;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightIntensity =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_INTENSITY);
	lightIntensity->SetMin(1);
	lightIntensity->SetMax(200);
	lightIntensity->SetSmallStep(1);
	lightIntensity->SetLargeStep(1);
	lightIntensity->SetPosition(1);
	lightIntensity->SetToolTipText(L"Set light intensity");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightDiffuseColorLine =
		AddStaticText(L"Diffuse:", screenRectangle, false, false, lightWindow, -1, false);
	lightDiffuseColorLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 80;
	screenRectangle.mExtent[0] = 5;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightDiffuseColorRedLine =
		AddStaticText(L"R:", screenRectangle, false, false, lightWindow, -1, false);
	lightDiffuseColorRedLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 115;
	screenRectangle.mExtent[0] = 40;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIEditBox> lightDiffuseColorRed = 
		AddEditBox(L"0", screenRectangle, true, true, lightWindow, CID_LIGHT_DIFFUSER);

	screenRectangle.mCenter[0] = 145;
	screenRectangle.mExtent[0] = 5;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightDiffuseColorGreenLine =
		AddStaticText(L"G:", screenRectangle, false, false, lightWindow, -1, false);
	lightDiffuseColorGreenLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 180;
	screenRectangle.mExtent[0] = 40;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIEditBox> lightDiffuseColorGreen =
		AddEditBox(L"0", screenRectangle, true, true, lightWindow, CID_LIGHT_DIFFUSEG);

	screenRectangle.mCenter[0] = 210;
	screenRectangle.mExtent[0] = 5;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightDiffuseColorBlueLine =
		AddStaticText(L"B:", screenRectangle, false, false, lightWindow, -1, false);
	lightDiffuseColorBlueLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 245;
	screenRectangle.mExtent[0] = 40;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIEditBox> lightDiffuseColorBlue =
		AddEditBox(L"0", screenRectangle, true, true, lightWindow, CID_LIGHT_DIFFUSEB);

	screenRectangle.mCenter[0] = 285;
	screenRectangle.mExtent[0] = 25;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIButton> lightDiffuseColorButton = 
		AddButton(screenRectangle, lightWindow, CID_SET_DIFFUSE_RADIO, L"set");
	lightDiffuseColorButton->SetDrawBorder(true);

	screenRectangle.mCenter[0] = 313;
	screenRectangle.mExtent[0] = 15;
	screenRectangle.mCenter[1] = 162;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightDiffuseColorResult =
		AddStaticText(L"", screenRectangle, true, false, lightWindow, CID_LIGHT_DIFFUSE, true);
	lightDiffuseColorResult->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);
	lightDiffuseColorResult->SetBackgroundColor(SColor(255, 255, 255, 255));

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightAmbientColorLine =
		AddStaticText(L"Ambient:", screenRectangle, false, false, lightWindow, -1, false);
	lightAmbientColorLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 80;
	screenRectangle.mExtent[0] = 5;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightAmbientColorRedLine =
		AddStaticText(L"R:", screenRectangle, false, false, lightWindow, -1, false);
	lightAmbientColorRedLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 115;
	screenRectangle.mExtent[0] = 40;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIEditBox> lightAmbientColorRed =
		AddEditBox(L"0", screenRectangle, true, true, lightWindow, CID_LIGHT_AMBIENTR);

	screenRectangle.mCenter[0] = 145;
	screenRectangle.mExtent[0] = 5;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightAmbientColorGreenLine =
		AddStaticText(L"G:", screenRectangle, false, false, lightWindow, -1, false);
	lightAmbientColorGreenLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 180;
	screenRectangle.mExtent[0] = 40;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIEditBox> lightAmbientColorGreen =
		AddEditBox(L"0", screenRectangle, true, true, lightWindow, CID_LIGHT_AMBIENTG);

	screenRectangle.mCenter[0] = 210;
	screenRectangle.mExtent[0] = 5;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightAmbientColorBlueLine =
		AddStaticText(L"B:", screenRectangle, false, false, lightWindow, -1, false);
	lightAmbientColorBlueLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 245;
	screenRectangle.mExtent[0] = 40;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIEditBox> lightAmbientColorBlue =
		AddEditBox(L"0", screenRectangle, true, true, lightWindow, CID_LIGHT_AMBIENTB);

	screenRectangle.mCenter[0] = 285;
	screenRectangle.mExtent[0] = 25;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIButton> lightAmbientColorButton =
		AddButton(screenRectangle, lightWindow, CID_SET_AMBIENT_RADIO, L"set");
	lightAmbientColorButton->SetDrawBorder(true);

	screenRectangle.mCenter[0] = 313;
	screenRectangle.mExtent[0] = 15;
	screenRectangle.mCenter[1] = 182;
	screenRectangle.mExtent[1] = 15;
	std::shared_ptr<BaseUIStaticText> lightAmbientColorResult =
		AddStaticText(L"", screenRectangle, true, false, lightWindow, CID_LIGHT_AMBIENT, true);
	lightAmbientColorResult->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);
	lightAmbientColorResult->SetBackgroundColor(SColor(255, 255, 255, 255));

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 212;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightFalloffLine =
		AddStaticText(L"Falloff", screenRectangle, false, false, lightWindow, -1, false);
	lightFalloffLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 232;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightConstantLine =
		AddStaticText(L"Constant:", screenRectangle, false, false, lightWindow, -1, false);
	lightConstantLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 232;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightConstant =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_CONSTANT);
	lightConstant->SetMin(5);
	lightConstant->SetMax(1000);
	lightConstant->SetSmallStep(1);
	lightConstant->SetLargeStep(1);
	lightConstant->SetPosition(100);
	lightConstant->SetToolTipText(L"Set light constant");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 252;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightLinearLine =
		AddStaticText(L"Linear:", screenRectangle, false, false, lightWindow, -1, false);
	lightLinearLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 252;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightLinear =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_LINEAR);
	lightLinear->SetMin(1);
	lightLinear->SetMax(4000);
	lightLinear->SetSmallStep(1);
	lightLinear->SetLargeStep(1);
	lightLinear->SetPosition(450);
	lightLinear->SetToolTipText(L"Set light linear");

	screenRectangle.mCenter[0] = 50;
	screenRectangle.mExtent[0] = 90;
	screenRectangle.mCenter[1] = 272;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIStaticText> lightQuadraticLine =
		AddStaticText(L"Quadratic:", screenRectangle, false, false, lightWindow, -1, false);
	lightQuadraticLine->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);

	screenRectangle.mCenter[0] = 200;
	screenRectangle.mExtent[0] = 240;
	screenRectangle.mCenter[1] = 272;
	screenRectangle.mExtent[1] = 16;
	std::shared_ptr<BaseUIScrollBar> lightQuadratic =
		AddScrollBar(true, true, screenRectangle, lightWindow, CID_LIGHT_QUADRATIC);
	lightQuadratic->SetMin(1);
	lightQuadratic->SetMax(1000000);
	lightQuadratic->SetSmallStep(1);
	lightQuadratic->SetLargeStep(1);
	lightQuadratic->SetPosition(75000);
	lightQuadratic->SetToolTipText(L"Set light quadratic");

	lightWindow->UpdateAbsoluteTransformation();

	screenRectangle = cameraWindow->GetRelativePosition();
	screenRectangle.mCenter[0] += 100;
	screenRectangle.mCenter[1] += 100;
	cameraWindow->SetRelativePosition(screenRectangle);

	screenRectangle = lightWindow->GetRelativePosition();
	screenRectangle.mCenter[0] -= 100;
	screenRectangle.mCenter[1] += 200;
	lightWindow->SetRelativePosition(screenRectangle);

	return true;
}

bool AreaLightsHUD::OnRestore()
{
	return BaseUI::OnRestore();
}

bool AreaLightsHUD::OnRender(double time, float elapsedTime)
{
	return BaseUI::OnRender(time, elapsedTime);
};


bool AreaLightsHUD::OnMsgProc( const Event& evt )
{
	return BaseUI::OnMsgProc( evt );
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
bool AreaLightsHUD::OnEvent(const Event& evt)
{
	if (evt.mEventType == ET_UI_EVENT)
	{
		switch (evt.mUIEvent.mCaller->GetID())
		{
			case CID_CAMERA_POSITION:
			{
				if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUIScrollBar>& cameraPosition =
						std::static_pointer_cast<BaseUIScrollBar>(
						root->GetElementFromId(CID_CAMERA_POSITION, true));

					AreaLightsApp* areaLightsApp = (AreaLightsApp*)Application::App;
					std::shared_ptr<CameraNode> camera = areaLightsApp->GetHumanView()->mCamera;
					camera->GetRelativeTransform().SetTranslation(
						(float)cameraPosition->GetPosition() * 0.1f * Vector4<float>::Unit(1));
				}
				break;
			}

			case CID_CAMERA_YAW:
			case CID_CAMERA_ROLL:
			case CID_CAMERA_PITCH:
			{
				if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUIScrollBar>& cameraYaw =
						std::static_pointer_cast<BaseUIScrollBar>(
						root->GetElementFromId(CID_CAMERA_YAW, true));
					const std::shared_ptr<BaseUIScrollBar>& cameraPitch =
						std::static_pointer_cast<BaseUIScrollBar>(
						root->GetElementFromId(CID_CAMERA_PITCH, true));
					const std::shared_ptr<BaseUIScrollBar>& cameraRoll =
						std::static_pointer_cast<BaseUIScrollBar>(
						root->GetElementFromId(CID_CAMERA_ROLL, true));

					AreaLightsApp* areaLightsApp = (AreaLightsApp*)Application::App;
					std::shared_ptr<CameraNode> camera = areaLightsApp->GetHumanView()->mCamera;
					Matrix4x4<float> yawRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(2), cameraYaw->GetPosition() * (float)GE_C_DEG_TO_RAD));
					Matrix4x4<float> pitchRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(0), -cameraPitch->GetPosition() * (float)GE_C_DEG_TO_RAD));
					Matrix4x4<float> rollRotation = Rotation<4, float>(
						AxisAngle<4, float>(Vector4<float>::Unit(1), cameraRoll->GetPosition() * (float)GE_C_DEG_TO_RAD));

					camera->GetRelativeTransform().SetRotation(yawRotation * pitchRotation * rollRotation);
				}
				break;
			}

			case CID_LIGHT_X:
			case CID_LIGHT_Y:
			case CID_LIGHT_Z:
			{
				if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUIScrollBar>& lightX =
						std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_LIGHT_X, true));
					const std::shared_ptr<BaseUIScrollBar>& lightY =
						std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_LIGHT_Y, true));
					const std::shared_ptr<BaseUIScrollBar>& lightZ =
						std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_LIGHT_Z, true));

					AreaLightsApp* areaLightsApp = (AreaLightsApp*)Application::App;
					AreaLightsHumanView* areaLightsView = (AreaLightsHumanView*)areaLightsApp->GetHumanView();
					std::shared_ptr<LightNode> lightNode = 
						std::static_pointer_cast<LightNode>(
						areaLightsView->mScene->GetSceneNode(areaLightsView->mLightID));
					lightNode->GetRelativeTransform().SetTranslation(
						Vector3<float>{
						(float)lightX->GetPosition() * 0.1f, 
						(float)lightY->GetPosition() * 0.1f, 
						(float)lightZ->GetPosition() * 0.1f});
				}
				break;
			}

			case CID_SET_DIFFUSE_RADIO:
			{
				if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUIStaticText>& lightDiffuse =
						std::static_pointer_cast<BaseUIStaticText>(root->GetElementFromId(CID_LIGHT_DIFFUSE, true));
					const std::shared_ptr<BaseUIEditBox>& lightDiffuseR =
						std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_LIGHT_DIFFUSER, true));
					const std::shared_ptr<BaseUIEditBox>& lightDiffuseG =
						std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_LIGHT_DIFFUSEG, true));
					const std::shared_ptr<BaseUIEditBox>& lightDiffuseB =
						std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_LIGHT_DIFFUSEB, true));

					int diffuseR = atoi(ToString(lightDiffuseR->GetText()).c_str());
					if (diffuseR > 255) diffuseR = 255;
					int diffuseG = atoi(ToString(lightDiffuseG->GetText()).c_str());
					if (diffuseG > 255) diffuseG = 255;
					int diffuseB = atoi(ToString(lightDiffuseB->GetText()).c_str());
					if (diffuseB > 255) diffuseB = 255;

					lightDiffuse->SetBackgroundColor(SColor(255, diffuseR, diffuseG, diffuseB));

					AreaLightsApp* areaLightsApp = (AreaLightsApp*)Application::App;
					AreaLightsHumanView* areaLightsView = (AreaLightsHumanView*)areaLightsApp->GetHumanView();
					std::shared_ptr<LightNode> lightNode =
						std::static_pointer_cast<LightNode>(
						areaLightsView->mScene->GetSceneNode(areaLightsView->mLightID));
					lightNode->GetLight()->mLighting->mDiffuse =
                    { diffuseR / 255.f, diffuseG / 255.f, diffuseB / 255.f, 1.f };
				}
				break;
			}

			case CID_SET_AMBIENT_RADIO:
			{
				if (evt.mUIEvent.mEventType == UIEVT_BUTTON_CLICKED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUIStaticText>& lightAmbient =
						std::static_pointer_cast<BaseUIStaticText>(root->GetElementFromId(CID_LIGHT_AMBIENT, true));
					const std::shared_ptr<BaseUIEditBox>& lightAmbientR =
						std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_LIGHT_AMBIENTR, true));
					const std::shared_ptr<BaseUIEditBox>& lightAmbientG =
						std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_LIGHT_AMBIENTG, true));
					const std::shared_ptr<BaseUIEditBox>& lightAmbientB =
						std::static_pointer_cast<BaseUIEditBox>(root->GetElementFromId(CID_LIGHT_AMBIENTB, true));

					int ambientR = atoi(ToString(lightAmbientR->GetText()).c_str());
					if (ambientR > 255) ambientR = 255;
					int ambientG = atoi(ToString(lightAmbientG->GetText()).c_str());
					if (ambientG > 255) ambientG = 255;
					int ambientB = atoi(ToString(lightAmbientB->GetText()).c_str());
					if (ambientB > 255) ambientB = 255;

					lightAmbient->SetBackgroundColor(SColor(255, ambientR, ambientG, ambientB));

					AreaLightsApp* areaLightsApp = (AreaLightsApp*)Application::App;
					AreaLightsHumanView* areaLightsView = (AreaLightsHumanView*)areaLightsApp->GetHumanView();
					std::shared_ptr<LightNode> lightNode =
						std::static_pointer_cast<LightNode>(
						areaLightsView->mScene->GetSceneNode(areaLightsView->mLightID));
					lightNode->GetLight()->mLighting->mAmbient =
					{ ambientR / 255.f, ambientG / 255.f, ambientB / 255.f, 1.f };
				}
				break;
			}

			case CID_LIGHT_LINEAR:
			case CID_LIGHT_CONSTANT:
			case CID_LIGHT_QUADRATIC:
			{
				if (evt.mUIEvent.mEventType == UIEVT_SCROLL_BAR_CHANGED)
				{
					const std::shared_ptr<BaseUIElement>& root = GetRootUIElement();
					const std::shared_ptr<BaseUIScrollBar>& lightLinear =
						std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_LIGHT_LINEAR, true));
					const std::shared_ptr<BaseUIScrollBar>& lightConstant =
						std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_LIGHT_CONSTANT, true));
					const std::shared_ptr<BaseUIScrollBar>& lightQuadratic =
						std::static_pointer_cast<BaseUIScrollBar>(root->GetElementFromId(CID_LIGHT_QUADRATIC, true));

					AreaLightsApp* areaLightsApp = (AreaLightsApp*)Application::App;
					AreaLightsHumanView* areaLightsView = (AreaLightsHumanView*)areaLightsApp->GetHumanView();
					std::shared_ptr<LightNode> lightNode =
						std::static_pointer_cast<LightNode>(
						areaLightsView->mScene->GetSceneNode(areaLightsView->mLightID));
					lightNode->GetLight()->mLighting->mAttenuation = 
					{ lightConstant->GetPosition() / 1000.f, lightLinear->GetPosition() / 4000.f, lightQuadratic->GetPosition() / 1000000.f, 1.0f };
				}
				break;
			}
		}
	}

	return BaseUI::OnEvent(evt);
}

//========================================================================
//
// AreaLightsHumanView Implementation
//
//========================================================================

//
// AreaLightsHumanView::AreaLightsHumanView	- Chapter 19, page 724
//
AreaLightsHumanView::AreaLightsHumanView() 
	: HumanView()
{ 
	mShowUI = true; 
	mDebugMode = DM_OFF;
	mLightID = INVALID_ACTOR_ID;
}


AreaLightsHumanView::~AreaLightsHumanView()
{

}

//
// AreaLightsHumanView::OnMsgProc				- Chapter 19, page 727
//
bool AreaLightsHumanView::OnMsgProc( const Event& evt )
{
	HumanView::OnMsgProc(evt);

	switch(evt.mEventType)
	{
		case ET_UI_EVENT:
			// hey, why is the user sending gui events..?
			break;

		case ET_KEY_INPUT_EVENT:
		{
			if (evt.mKeyInput.mPressedDown)
			{
				switch (evt.mKeyInput.mKey)
				{
					case KEY_KEY_6:
					{
						mDebugMode = mDebugMode ? DM_OFF : DM_WIREFRAME;
						for (auto child : mScene->GetRootNode()->GetChildren())
							child->SetDebugState(mDebugMode);
						return true;
					}

					case KEY_ESCAPE:
					{
						WindowApplication* windowApp = (WindowApplication*)Application::App;
						windowApp->SetQuitting(true);
						return true;
					}
				}
			}
		}
	}

	return false;
}

//
// AreaLightsHumanView::RenderText				- Chapter 19, page 727
//
void AreaLightsHumanView::RenderText()
{
	HumanView::RenderText();
}


//
// AreaLightsHumanView::OnUpdate				- Chapter 19, page 730
//
void AreaLightsHumanView::OnUpdate(unsigned int timeMs, unsigned long deltaMs)
{
	HumanView::OnUpdate( timeMs, deltaMs );
}

//
// AreaLightsHumanView::OnAttach				- Chapter 19, page 731
//
void AreaLightsHumanView::OnAttach(GameViewId vid, ActorId aid)
{
	HumanView::OnAttach(vid, aid);
}

void GetRandomAxisAngle(AxisAngle<4, float>& axisAngle)
{
	float yaw = (float)GE_C_TWO_PI * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
	float roll = (float)GE_C_TWO_PI * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
	float pitch = (float)GE_C_TWO_PI * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
	Matrix4x4<float> yawRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(2), yaw));
	Matrix4x4<float> rollRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(1), roll));
	Matrix4x4<float> pitchRotation = Rotation<4, float>(
		AxisAngle<4, float>(Vector4<float>::Unit(0), pitch));

	Transform rotation;
	rotation.SetRotation(yawRotation * pitchRotation * rollRotation);
	rotation.GetRotation(axisAngle);
}

bool AreaLightsHumanView::LoadGameDelegate(tinyxml2::XMLElement* pLevelData)
{
	if (!HumanView::LoadGameDelegate(pLevelData))
		return false;

    mAreaLightsHUD.reset(new AreaLightsHUD());
	mAreaLightsHUD->OnInit();
    PushElement(mAreaLightsHUD);

    // A movement controller is going to control the camera, 
    // but it could be constructed with any of the objects you see in this function.
	mCamera->GetAbsoluteTransform().SetTranslation(Vector4<float>::Zero());
	mCamera->ClearTarget();

	std::shared_ptr<ResHandle>& resHandle =
		ResCache::Get()->GetHandle(&BaseResource(L"art/stones.jpg"));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();

		for (auto i = 0; i < 160; i++)
		{
			// create an animated mesh scene node with specified mesh.
			float xSize = 0.1f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
			float ySize = 0.1f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
			float zSize = 0.1f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
			xSize = xSize == 0.f ? 0.01f : xSize;
			ySize = ySize == 0.f ? 0.01f : ySize;
			zSize = zSize == 0.f ? 0.01f : zSize;
			std::shared_ptr<Node> boxNode = mScene->AddBoxNode(
				0, extra->GetImage(), { 1.0f, 1.0f }, { xSize, ySize, zSize }, GameLogic::Get()->GetNewActorID());

			//To let the mesh look a little bit nicer, we change its material. We
			//disable lighting because we do not have a dynamic light in here, and
			//the mesh would be totally black otherwise. And last, we apply a
			//texture to the mesh. Without it the mesh would be drawn using only a color.
			if (boxNode)
			{
				for (unsigned int i = 0; i < boxNode->GetMaterialCount(); ++i)
					boxNode->GetMaterial(i)->mLighting = false;
				boxNode->SetMaterialTexture(0, extra->GetImage());

				AxisAngle<4, float> axisAngles;
				GetRandomAxisAngle(axisAngles);
				std::shared_ptr<NodeAnimator> anim = 0;
				anim = mScene->CreateRotationAnimator(axisAngles.mAxis, 0.1f);
				boxNode->AttachAnimator(anim);

				GetRandomAxisAngle(axisAngles);
				Vector3<float> direction{ axisAngles.mAxis[0], axisAngles.mAxis[1], axisAngles.mAxis[2] };
				float start = ((Randomizer::Rand() & 0x7fff) / (float)0x7fff);
				float radius = 1.f;
				float distance = 4.f * ((Randomizer::Rand() & 0x7fff) / (float)0x7fff) + 2.f;
				anim = mScene->CreateFlyCircleAnimator(
					Vector3<float>::Unit(1) * distance, radius, 0.0001f, direction, start);
				boxNode->AttachAnimator(anim);
			}
		}
	}

	std::shared_ptr<Light> lightData;
#if defined(_OPENGL_)
	lightData = std::make_shared<Light>(true, false);
#else
	lightData = std::make_shared<Light>(true, true);
#endif
	lightData->mLighting = std::make_shared<Lighting>();
	lightData->mLighting->mAmbient = { 0.05f, 0.05f, 0.05f };
	lightData->mLighting->mAttenuation = { 1.0f, 0.045f, 0.0075f, 1.0f };

	// Add light
	mLightID = GameLogic::Get()->GetNewActorID();
	std::shared_ptr<Node> lightNode = mScene->AddLightNode(0, lightData, mLightID);
	if (lightNode)
	{
		resHandle = ResCache::Get()->GetHandle(&BaseResource(L"art/particlewhite.bmp"));
		if (resHandle)
		{
			const std::shared_ptr<ImageResourceExtraData>& extra =
				std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
			extra->GetImage()->AutogenerateMipmaps();

			std::shared_ptr<Node> billboardNode = mScene->AddBillboardNode(
				lightNode, extra->GetImage(), Vector2<float>{1.0f, 1.0f}, GameLogic::Get()->GetNewActorID());
			if (billboardNode)
			{
				billboardNode->GetRelativeTransform().SetTranslation(Vector3<float>{0.f, 4.f, 0.f});

				for (unsigned int i = 0; i < billboardNode->GetMaterialCount(); ++i)
					billboardNode->GetMaterial(i)->mLighting = false;
				billboardNode->SetMaterialType(MT_TRANSPARENT);

				for (unsigned int i = 0; i < billboardNode->GetMaterialCount(); ++i)
				{
					std::shared_ptr<Material> material = billboardNode->GetMaterial(i);
					material->mBlendTarget.enable = true;
					material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
					material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
					material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
					material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

					material->mDepthBuffer = true;
					material->mDepthMask = DepthStencilState::MASK_ZERO;

					material->mFillMode = RasterizerState::FILL_SOLID;
					material->mCullMode = RasterizerState::CULL_NONE;
				}
			}
		}
	}

    mScene->OnRestore();
    return true;
}