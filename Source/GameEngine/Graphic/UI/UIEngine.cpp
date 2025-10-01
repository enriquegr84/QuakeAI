//========================================================================
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
//    http://code.google.com/p/gamecode4/
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

#include "UIEngine.h"

#include "Core/IO/XmlResource.h"
#include "Core/OS/Os.h"

#include "Graphic/Image/ImageResource.h"

//! constructor
BaseUI::BaseUI()
	: mVisible(true), mHovered(0), mHoveredNoSubelement(0), mFocus(0),
    mFocusFlags(UIFF_SET_ON_LMOUSE_DOWN | UIFF_SET_ON_TAB),
	mLastHoveredMousePos{ 0,0 }, mCurrentSkin(0)
{

    // basic color effect
    {
        mBlendState = std::make_shared<BlendState>();
        mBlendState->mTarget[0].enable = true;
        mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
        mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
        mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        // Create a vertex buffer for a single triangle.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

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

        std::shared_ptr<VisualEffect> effect =
            std::make_shared<ColorEffect>(ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
        mVisual = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    }

    std::shared_ptr<ResHandle>& resHandle =
        ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
    if (resHandle)
    {
        const std::shared_ptr<ImageResourceExtraData>& extra =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        extra->GetImage()->AutogenerateMipmaps();

        // Create a vertex buffer for a two-triangles square. The PNG is stored
        // in left-handed coordinates. The texture coordinates are chosen to
        // reflect the texture in the y-direction.
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

        std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
        std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
        vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

        // Create an effect for the vertex and pixel shaders. The texture is
        // bilinearly filtered and the texture coordinates are clamped to [0,1]^2.
        std::vector<std::string> path;
#if defined(_OPENGL_)
        path.push_back("Effects/Texture2ColorEffectVS.glsl");
        path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
        path.push_back("Effects/Texture2ColorEffectVS.hlsl");
        path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		resHandle = ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extraRes =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extraRes->GetProgram())
			extraRes->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

        mEffect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extraRes->GetProgram()), extra->GetImage(),
            SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::CLAMP, SamplerState::CLAMP);

        // Create the geometric object for drawing.
        mVisualLayout = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
    }


	Renderer* renderer = Renderer::Get();
	Vector2<unsigned int> screenSize(renderer->GetScreenSize());
	RectangleShape<2, int> screenRectangle;
	screenRectangle.mCenter[0] = screenSize[0] / 2;
	screenRectangle.mCenter[1] = screenSize[1] / 2;
	screenRectangle.mExtent[0] = (int)screenSize[0];
	screenRectangle.mExtent[1] = (int)screenSize[1];

	mRoot = std::shared_ptr<BaseUIElement>(new UIRoot(this, UIET_ROOT, 0, screenRectangle));

	// environment is root tab group
	mRoot->SetTabGroup(true);
}


//! destructor
BaseUI::~BaseUI()
{
    for (UITexture& texture : mTextureLayer)
        texture.mTexture = nullptr;
}

bool BaseUI::OnInit()
{
	//LoadBuiltInFont
	std::vector<std::string> path;
#if defined(_OPENGL_)
	path.push_back("Effects/TextEffectVS.glsl");
	path.push_back("Effects/TextEffectPS.glsl");
#else
	path.push_back("Effects/TextEffectVS.hlsl");
	path.push_back("Effects/TextEffectPS.hlsl");
#endif
	std::shared_ptr<ResHandle> resHandle =
		ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

	const std::shared_ptr<ShaderResourceExtraData>& extra =
		std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
	if (!extra->GetProgram())
		extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

	std::shared_ptr<Font> builtInFont = std::make_shared<FontArialW400H18>(
		ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), 256);
	Renderer::Get()->SetDefaultFont(builtInFont);
	mFonts[L"DefaultFont"] = 
		std::shared_ptr<BaseUIFont>(new UIFont(this, L"DefaultFont", builtInFont));

	const std::shared_ptr<BaseUISkin>& skin = CreateSkin( STT_WINDOWS_METALLIC );
	SetSkin(skin);

    //create title text
    RectangleShape<2, int> rect;
    rect.mExtent = GetSkin()->GetFont(DF_WINDOW)->GetDimension(L" ");
    rect.mCenter = rect.mExtent / 2;
    rect.mCenter += Vector2<int>{4, 0};
    mTitleText = AddStaticText(L"", rect);

	//set tooltip default
	mToolTip.mLastTime = 0;
	mToolTip.mEnterTime = 0;
	mToolTip.mLaunchTime = 1000;
	mToolTip.mRelaunchTime = 500;
	mToolTip.mElement = 
		AddStaticText(L"", RectangleShape<2, int>(), true, true, mRoot, -1, true);

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_MAXIMIZE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_MAXIMIZE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_RESTORE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_RESTORE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_CLOSE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_CLOSE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_MINIMIZE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_MINIMIZE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_RESIZE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_RESIZE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_COLLAPSE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_COLLAPSE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_WINDOW_EXPAND)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_WINDOW_EXPAND));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_CURSOR_UP)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_CURSOR_UP));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_CURSOR_DOWN)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_CURSOR_DOWN));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_CURSOR_LEFT)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_CURSOR_LEFT));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}
	
	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_CURSOR_RIGHT)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_CURSOR_RIGHT));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_MENU_MORE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_MENU_MORE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_CHECKBOX_CHECKED)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_CHECKBOX_CHECKED));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_CHECKBOX_UNCHECKED)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_CHECKBOX_UNCHECKED));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_DROP_DOWN)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_DROP_DOWN));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_SMALL_CURSOR_UP)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_SMALL_CURSOR_UP));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_SMALL_CURSOR_DOWN)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_SMALL_CURSOR_DOWN));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_RADIO_BUTTON_CHECKED)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_RADIO_BUTTON_CHECKED));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_MORE_LEFT)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_MORE_LEFT));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_MORE_RIGHT)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_MORE_RIGHT));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_MORE_UP)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_MORE_UP));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_MORE_DOWN)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_MORE_DOWN));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_EXPAND)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_EXPAND));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	resHandle = ResCache::Get()->GetHandle(&BaseResource(mCurrentSkin->GetIcon(DI_COLLAPSE)));
	if (resHandle)
	{
		const std::shared_ptr<ImageResourceExtraData>& extra =
			std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
		extra->GetImage()->AutogenerateMipmaps();
		extra->GetImage()->SetName(mCurrentSkin->GetIcon(DI_COLLAPSE));
		mCurrentSkin->GetSpriteBank()->AddTextureAsSprite(extra->GetImage());
	}

	return true;
}

//! draws all gui elements
bool BaseUI::OnRender(double time, float elapsedTime)
{
	Renderer* renderer = Renderer::Get();
    renderer->SetBlendState(mBlendState);

    if (!DrawBackground())
        DrawOverlay();
    DrawHeader();
    DrawFooter();

    renderer->SetDefaultBlendState();

	Vector2<unsigned int> screenSize(renderer->GetScreenSize());
	if (mRoot->mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[0] != screenSize[0] ||
		mRoot->mAbsoluteRect.GetVertice(RVP_LOWERRIGHT)[1] != screenSize[1])
	{
		// resize gui environment
		Vector2<int> center(mRoot->mDesiredRect.mCenter);
		mRoot->mDesiredRect.mCenter[0] = 
			(int)screenSize[0] - (int)round(mRoot->mDesiredRect.mExtent[0] / 2.f);
		mRoot->mDesiredRect.mCenter[1] = 
			(int)screenSize[1] - (int)round(mRoot->mDesiredRect.mExtent[1] / 2.f);
		mRoot->mDesiredRect.mExtent[0] = 2 * ((int)screenSize[0] - center[0]);
		mRoot->mDesiredRect.mExtent[1] = 2 * ((int)screenSize[1] - center[1]);
		mRoot->mAbsoluteClippingRect = mRoot->mDesiredRect;
		mRoot->mAbsoluteRect = mRoot->mDesiredRect;
		mRoot->UpdateAbsolutePosition();
	}

	// make sure tooltip is always on top
	if (mToolTip.mElement && mToolTip.mElement->IsVisible())
		mRoot->BringToFront(mToolTip.mElement);

	mRoot->Draw();

	return OnPostRender ( Timer::GetTime () );
}


//! clear all UI elements
void BaseUI::Clear()
{
	// Remove the focus
	if (mFocus)
		mFocus = 0;

	if (mHovered && mHovered != mRoot)
		mHovered = 0;
	
	if ( mHoveredNoSubelement && mHoveredNoSubelement != mRoot)
		mHoveredNoSubelement = 0;

	// get the root's children in case the root changes in future
	const std::list<std::shared_ptr<BaseUIElement>>& children =
		GetRootUIElement()->GetChildren();

	while (!children.empty())
		children.back()->Remove();
}

//
bool BaseUI::OnPostRender( unsigned int time )
{
	// launch tooltip
	if (mToolTip.mElement && !mToolTip.mElement->IsVisible() &&
		mHoveredNoSubelement && mHoveredNoSubelement != mRoot &&
		(time - mToolTip.mEnterTime >= mToolTip.mLaunchTime || 
		(time - mToolTip.mLastTime >= mToolTip.mRelaunchTime && 
		time - mToolTip.mLastTime < mToolTip.mLaunchTime)) &&
		!mHoveredNoSubelement->GetToolTipText().empty() &&
		GetSkin() && GetSkin()->GetFont(DF_TOOLTIP))
	{
		RectangleShape<2, int> pos;

		Vector2<int> dim = GetSkin()->GetFont(DF_TOOLTIP)->GetDimension(
			mHoveredNoSubelement->GetToolTipText());
		dim[0] += GetSkin()->GetSize(DS_TEXT_DISTANCE_X) * 2;
		dim[1] += GetSkin()->GetSize(DS_TEXT_DISTANCE_Y) * 2;

		pos.mCenter[0] = mLastHoveredMousePos[0] + (dim[0] / 2);
		pos.mExtent[0] = dim[0];
		pos.mCenter[1] = mLastHoveredMousePos[1] - (dim[1] / 2);
		pos.mExtent[1] = dim[1];

		mToolTip.mElement->SetVisible(true);
		mToolTip.mElement->SetRelativePosition(pos);
		mToolTip.mElement->SetTextAlignment(UIA_UPPERLEFT, UIA_CENTER);
		mToolTip.mElement->SetText(mHoveredNoSubelement->GetToolTipText().c_str());
		mToolTip.mElement->SetOverrideColor(GetSkin()->GetColor(DC_TOOLTIP));
		mToolTip.mElement->SetBackgroundColor(GetSkin()->GetColor(DC_TOOLTIP_BACKGROUND));
		mToolTip.mElement->SetOverrideFont(GetSkin()->GetFont(DF_TOOLTIP));
		mToolTip.mElement->SetSubElement(true);
	}

	// (IsVisible() check only because we might use visibility for ToolTip one day)
	if (mToolTip.mElement && mToolTip.mElement->IsVisible())
	{
		mToolTip.mLastTime = time;

		// got invisible or removed in the meantime?
		if (!mHoveredNoSubelement ||
			!mHoveredNoSubelement->IsVisible() ||
			!mHoveredNoSubelement->GetParent())
		{
			mToolTip.mElement->SetVisible(false);
		}
	}

	mRoot->OnPostDraw ( time );
	return true;
}


//
void BaseUI::UpdateHoveredElement(Vector2<int> mousePos)
{
	std::shared_ptr<BaseUIElement> lastHovered = mHovered;
	std::shared_ptr<BaseUIElement> lastHoveredNoSubelement = mHoveredNoSubelement;
	mLastHoveredMousePos = mousePos;

	mHovered = mRoot->GetElementFromPoint(mousePos);

	if (mToolTip.mElement && mHovered == mToolTip.mElement)
	{
		// When the mouse is over the ToolTip we remove that so it will be re-created 
		// at a new position. Note that ToolTip.EnterTime does not get changed here, 
		// so it will be re-created at once.
		mToolTip.mElement->SetVisible(false);
        //mToolTip.mElement->Remove();
        //mToolTip.mElement = nullptr;

		// Get the real Hovered
		mHovered = mRoot->GetElementFromPoint(mousePos);
		//mHovered = lastHovered;
	}

	// for tooltips we want the element itself and not some of it's subelements
	mHoveredNoSubelement = mHovered;
	while ( mHoveredNoSubelement && mHoveredNoSubelement->IsSubElement() )
	{
		mHoveredNoSubelement = mHoveredNoSubelement->GetParent();
	}

	if (mHovered != lastHovered)
	{
		Event ev;
		ev.mEventType = ET_UI_EVENT;

		if (lastHovered)
		{
			ev.mUIEvent.mCaller = lastHovered.get();
			ev.mUIEvent.mElement = 0;
			ev.mUIEvent.mEventType = UIEVT_ELEMENT_LEFT;
            if (!lastHovered->OnPreEvent(ev))
                lastHovered->OnEvent(ev);
		}

		if ( mHovered )
		{
			ev.mUIEvent.mCaller  = mHovered.get();
			ev.mUIEvent.mElement = mHovered.get();
			ev.mUIEvent.mEventType = UIEVT_ELEMENT_HOVERED;
            if (!mHovered->OnPreEvent(ev))
                mHovered->OnEvent(ev);
		}
	}

	if (lastHoveredNoSubelement != mHoveredNoSubelement)
	{
		if (mToolTip.mElement)
			mToolTip.mElement->SetVisible(false);

		if (mHoveredNoSubelement)
		{
			unsigned int now = Timer::GetTime();
			mToolTip.mEnterTime = now;
		}
	}
}

//! posts an input event to the environment
bool BaseUI::OnMsgProc(const Event& evt)
{
	switch(evt.mEventType)
	{
		case ET_UI_EVENT:
			// hey, why is the user sending gui events..?
			break;
		case ET_MOUSE_INPUT_EVENT:

			UpdateHoveredElement(Vector2<int>{evt.mMouseInput.X, evt.mMouseInput.Y});

            if (mHovered != mFocus)
            {
                std::shared_ptr<BaseUIElement> focusCandidate = mHovered;

                // Only allow enabled elements to be focused (unless UIFF_CAN_FOCUS_DISABLED is set)
                if (mHovered && !mHovered->IsEnabled() && !(mFocusFlags & UIFF_CAN_FOCUS_DISABLED))
                    focusCandidate = NULL;	// we still remove focus from the active element

                // Please don't merge this into a single if clause, it's easier to debug the way it is
                if (mFocusFlags & UIFF_SET_ON_LMOUSE_DOWN &&
					evt.mMouseInput.mEvent == MIE_LMOUSE_PRESSED_DOWN)
                {
                    SetFocus(focusCandidate);
                }
                else if (mFocusFlags & UIFF_SET_ON_RMOUSE_DOWN &&
					evt.mMouseInput.mEvent == MIE_RMOUSE_PRESSED_DOWN)
                {
                    SetFocus(focusCandidate);
                }
                else if (mFocusFlags & UIFF_SET_ON_MOUSE_OVER &&
					evt.mMouseInput.mEvent == MIE_MOUSE_MOVED)
                {
                    SetFocus(focusCandidate);
                }
            }

			// sending input to focus
			if (mFocus && !mFocus->OnPreEvent(evt) && mFocus->OnEvent(evt))
				return true;

			// focus could have died in last call
			if (!mFocus && mHovered && !mHovered->OnPreEvent(evt))
				return mHovered->OnEvent(evt);

			break;
		case ET_KEY_INPUT_EVENT:
			{
				if (mFocus && !mFocus->OnPreEvent(evt) && mFocus->OnEvent(evt))
					return true;

				// For keys we handle the event before changing focus to give elements 
				// the chance for catching the TAB. Send focus changing event
				if (evt.mEventType == ET_KEY_INPUT_EVENT &&
					evt.mKeyInput.mPressedDown &&
					evt.mKeyInput.mKey == KEY_TAB)
				{
					const std::shared_ptr<BaseUIElement>& next =
						GetNextElement(evt.mKeyInput.mShift, evt.mKeyInput.mControl);
					if (next && next != mFocus)
					{
						if (SetFocus(next))
							return true;
					}
				}

			}
			break;
        case ET_STRING_INPUT_EVENT:
            if (mFocus && !mFocus->OnPreEvent(evt) && mFocus->OnEvent(evt))
                return true;
            break;
		default:
			break;
	} // end switch

	return false;
}


//! sets the focus to an element
bool BaseUI::SetFocus(std::shared_ptr<BaseUIElement> element)
{
	if (mFocus == element)
		return false;

	// UI Environment should not get the focus
	if (element == mRoot)
		element = 0;

	// focus may change or be removed in this call
	std::shared_ptr<BaseUIElement> currentFocus = 0;
	if (mFocus)
	{
		currentFocus = mFocus;
		Event ev;
		ev.mEventType = ET_UI_EVENT;
		ev.mUIEvent.mCaller = mFocus.get();
		ev.mUIEvent.mElement = element.get();
		ev.mUIEvent.mEventType = UIEVT_ELEMENT_FOCUS_LOST;
        if (mFocus->OnPreEvent(ev) || mFocus->OnEvent(ev))
			return false;

		currentFocus = 0;
	}

	if (element)
	{
		currentFocus = mFocus;

		// send Focused event
		Event ev;
		ev.mEventType = ET_UI_EVENT;
		ev.mUIEvent.mCaller = element.get();
		ev.mUIEvent.mElement = mFocus.get();
		ev.mUIEvent.mEventType = UIEVT_ELEMENT_FOCUSED;
        if (element->OnPreEvent(ev) || element->OnEvent(ev))
			return false;
	}

	// element is the new focus so it doesn't have to be dropped
	mFocus = element;

	return true;
}


//! returns the element with the focus
const std::shared_ptr<BaseUIElement>& BaseUI::GetFocus() const
{
	return mFocus;
}


//! returns the element last known to be under the mouse cursor
const std::shared_ptr<BaseUIElement>& BaseUI::GetHovered() const
{
	return mHovered;
}


//! removes the hovered element
bool BaseUI::RemoveHovered()
{
	if (mHovered && mHovered != mRoot)
		mHovered = 0;

	if (mHoveredNoSubelement && mHoveredNoSubelement != mRoot)
		mHoveredNoSubelement = 0;

	return true;
}


//! removes the focus from an element
bool BaseUI::RemoveFocus(const std::shared_ptr<BaseUIElement>& element)
{
	if (mFocus && mFocus==element)
	{
		Event ev;
		ev.mEventType = ET_UI_EVENT;
		ev.mUIEvent.mCaller = mFocus.get();
		ev.mUIEvent.mElement = 0;
		ev.mUIEvent.mEventType = UIEVT_ELEMENT_FOCUS_LOST;
        if (mFocus->OnPreEvent(ev) || mFocus->OnEvent(ev))
			return false;
	}

	if (mFocus)
		mFocus = 0;

	return true;
}


//! Returns whether the element has focus
bool BaseUI::HasFocus(const std::shared_ptr<BaseUIElement>& element, bool checkSubElements) const
{
	if (element && element == mFocus)
		return true;

	if ( !checkSubElements || !element )
		return false;

	std::shared_ptr<BaseUIElement> f = mFocus;
	while ( f && f->IsSubElement() )
	{
		f = f->GetParent();
		if ( f == element )
			return true;
	}
	return false;
}


//! returns the current gui skin
const std::shared_ptr<BaseUISkin>& BaseUI::GetSkin() const
{
	return mCurrentSkin;
}


//! Sets a new UI Skin
void BaseUI::SetSkin(const std::shared_ptr<BaseUISkin>& skin)
{
	mCurrentSkin = skin;
}


//! Creates a new UI Skin based on a template.
// \return Returns a pointer to the created skin.
//	If you no longer need the skin, you should call BaseUISkin::drop().
//	See IReferenceCounted::drop() for more information.
std::shared_ptr<BaseUISkin> BaseUI::CreateSkin(UISkinThemeType type)
{
	std::shared_ptr<UISkin> skin(new UISkin(this, type));
	/*
	To make the font a little bit nicer, we load an external font
	and set it as the new default font in the skin.
	*/
	std::shared_ptr<BaseUIFont> font(GetFont(L"DefaultFont"));
	if (font) skin->SetFont(font);

	BaseUIFontBitmap* bitfont = 0;
	if (font && font->GetType() == FT_BITMAP)
		bitfont = (BaseUIFontBitmap*)font.get();

	skin->SetFont(font);

	std::shared_ptr<BaseUISpriteBank> bank = 0;
	if (bitfont)
		bank = bitfont->GetSpriteBank();
	skin->SetSpriteBank(bank);

	return skin;
}


//! returns the font
std::shared_ptr<BaseUIFont> BaseUI::GetFont(const std::wstring& fileName)
{
	auto itFont = mFonts.find(fileName);
	if (itFont != mFonts.end()) return mFonts[fileName];

	tinyxml2::XMLElement* pRoot = XmlResourceLoader::LoadAndReturnRootXMLElement(fileName.c_str());
    // font doesn't exist, attempt to load it
	if (!pRoot)
    {
        LogError(L"Failed to find resource file: " + fileName);
        return nullptr;
    }
	
	bool found=false;
	// this is an XML font, but we need to know what type
	UIFontType t = FT_CUSTOM;
	// Loop through each child element and load the component
    for (tinyxml2::XMLElement* pNode = pRoot->FirstChildElement(); pNode; pNode = pNode->NextSiblingElement())
    {
		if (std::string("font").compare(std::string(pNode->Value())) == 0)
		{
			std::string type(pNode->Attribute("type"));
			if (std::string("vector").compare(type) == 0)
			{
				t = FT_VECTOR;
				found=true;
			}
			else if (std::string("bitmap").compare(type) == 0)
			{
				t = FT_BITMAP;
				found=true;
			}
			else found=true;
		}
        unsigned char flags = 0;
      
    }

	std::shared_ptr<UIFont> font=0;
	if (t==FT_BITMAP)
	{
		font = std::shared_ptr<UIFont>(new UIFont(this, fileName));
		// change working directory, for loading textures
		std::wstring workingDir = FileSystem::Get()->GetWorkingDirectory();
		FileSystem::Get()->ChangeWorkingDirectoryTo(FileSystem::Get()->GetFileDirectory(fileName));

		// load the font
		if (!((UIFont*)font.get())->Load(fileName.c_str()))
			font = 0;

		// change working dir back again
		FileSystem::Get()->ChangeWorkingDirectoryTo( workingDir );
	}
	else if (t==FT_VECTOR)
	{
		// todo: vector fonts
		LogError(L"Unable to load font, XML vector fonts are not supported yet " + fileName);

		//UIFontVector* fontVector = new UIFontVector(Driver);
		//font = std::shared_ptr<BaseUIFont>(font);
		//if (!font->Load(pRoot))
	}

	if (!font)
	{
		font = std::shared_ptr<UIFont>(new UIFont(this, fileName));
		if (!font->Load(fileName))
		{
			font = 0;
			return nullptr;
		}
	}

	// add to fonts.
	mFonts[fileName] = font;
	return font;
}


//! add an externally loaded font
const std::shared_ptr<BaseUIFont>& BaseUI::AddFont(
	const std::wstring& name, const std::shared_ptr<BaseUIFont>& font)
{
	if (font)
	{
		auto itFont = mFonts.find(name);
		if (itFont == mFonts.end()) mFonts[name] = font;
	}

	return font;
}


//! remove loaded font
void BaseUI::RemoveFont(const std::shared_ptr<BaseUIFont>& font)
{
	if ( !font )
		return;

	auto itFont = mFonts.begin();
	for (; itFont != mFonts.end(); itFont++)
	{
		if ((*itFont).second == font)
		{
			mFonts.erase(itFont);
			return;
		}
	}
}


//! returns default font
std::shared_ptr<BaseUIFont> BaseUI::GetBuiltInFont()
{
	if (mFonts.empty())
		return nullptr;

	return mFonts[L"DefaultFont"];
}


std::shared_ptr<BaseUISpriteBank> BaseUI::GetSpriteBank(const std::wstring& fileName)
{
	auto itBank = mBanks.find(fileName);
	if (itBank != mBanks.end()) return mBanks[fileName];

	return nullptr;
}


std::shared_ptr<BaseUISpriteBank> BaseUI::AddEmptySpriteBank(const std::wstring& fileName)
{
	// no duplicate names allowed
	auto itBank = mBanks.find(fileName);
	if (itBank != mBanks.end()) return mBanks[fileName];

	mBanks[fileName] = std::shared_ptr<UISpriteBank>(new UISpriteBank(this));
	return mBanks[fileName];
}


//! Returns the default element factory which can create all built in elements
std::shared_ptr<UIElementFactory> BaseUI::GetDefaultUIElementFactory()
{
	// gui factory
	if (!GetUIElementFactory(0))
	{
		std::shared_ptr<UIElementFactory> factory(new DefaultUIElementFactory(this));
		RegisterUIElementFactory(factory);
	}
	return GetUIElementFactory(0);
}


//! Adds an element factory to the gui environment.
// Use this to extend the gui environment with new element types which it should be
// able to create automaticly, for example when loading data from xml files.
void BaseUI::RegisterUIElementFactory(const std::shared_ptr<UIElementFactory>& factoryToAdd)
{
	if (factoryToAdd)
		UIElementFactoryList.push_back(factoryToAdd);
}


//! Returns amount of registered scene node factories.
size_t BaseUI::GetRegisteredUIElementFactoryCount() const
{
	return UIElementFactoryList.size();
}


//! Returns a scene node factory by index
std::shared_ptr<UIElementFactory> BaseUI::GetUIElementFactory(unsigned int index) const
{
	if (index < UIElementFactoryList.size())
		return UIElementFactoryList[index];
	else
		return nullptr;
}


//! Returns the root gui element.
const std::shared_ptr<BaseUIElement>& BaseUI::GetRootUIElement()
{
	return mRoot;
}


//! Returns the next element in the tab group starting at the Focused element
std::shared_ptr<BaseUIElement> BaseUI::GetNextElement(bool reverse, bool group)
{
	// start the search at the root of the current tab group
	std::shared_ptr<BaseUIElement> startPos = mFocus ? mFocus->GetTabGroup() : 0;
	int startOrder = -1;

	// if we're searching for a group
	if (group && startPos)
	{
		startOrder = startPos->GetTabOrder();
	}
	else if (!group && mFocus && !mFocus->IsTabGroup())
	{
		startOrder = mFocus->GetTabOrder();
		if (startOrder == -1)
		{
			// this element is not part of the tab cycle,
			// but its parent might be...
			std::shared_ptr<BaseUIElement> el = mFocus;
			while (el && el->GetParent() && startOrder == -1)
			{
				el = el->GetParent();
				startOrder = el->GetTabOrder();
			}
		}
	}

	if (group || !startPos)
		startPos = mRoot; // start at the root

	// find the element
	std::shared_ptr<BaseUIElement> closest = 0;
	std::shared_ptr<BaseUIElement> first = 0;
	startPos->GetNextElement(startOrder, reverse, group, first, closest);

	if (closest)
		return closest; // we found an element
	else if (first)
		return first; // go to the end or the start
	else if (group)
		return mRoot; // no group found? root group
	else
		return nullptr;
}


//! adds a UI Element using its name
std::shared_ptr<BaseUIElement> BaseUI::AddUIElement(UIElementType elementType,
	const std::shared_ptr<BaseUIElement>& parent)
{
	std::shared_ptr<BaseUIElement> node=0;

	for (int i= (int)UIElementFactoryList.size()-1; i>=0 && !node; --i)
		node = UIElementFactoryList[i]->AddUIElement(elementType, parent ? parent : mRoot);

	return node;
}


//! adds a box
std::shared_ptr<BaseUIBox> BaseUI::AddBox(const RectangleShape<2, int>& rectangle,
        const std::array<SColor, 4>& colors,
        const std::array<SColor, 4>& bordercolors,
        const std::array<int, 4>& borderwidths,
        const std::shared_ptr<BaseUIElement>& parent, int id)
{
    std::shared_ptr<BaseUIBox> box(new UIBox(this, id, rectangle, colors, bordercolors, borderwidths));
    box->SetParent(parent ? parent : mRoot);

    return box;
}


//! adds a button. The returned pointer must not be dropped.
std::shared_ptr<BaseUIButton> BaseUI::AddButton(const RectangleShape<2, int>& rectangle,
	const std::shared_ptr<BaseUIElement>& parent, int id, const wchar_t* text, 
    const wchar_t *tooltiptext, bool noClip, bool foreGroundImage)
{
	std::shared_ptr<BaseUIButton> button(new UIButton(this, id, rectangle));
	button->SetParent(parent ? parent : mRoot);
	button->OnInit(noClip, foreGroundImage);
	if (text)
		button->SetText(text);

	if (tooltiptext)
		button->SetToolTipText(tooltiptext);

	return button;
}


//! adds a static text. The returned pointer must not be dropped.
std::shared_ptr<BaseUIStaticText> BaseUI::AddStaticText(const wchar_t* text, const RectangleShape<2, int>& rectangle,
	bool border, bool wordWrap, const std::shared_ptr<BaseUIElement>& parent, int id, bool background)
{
	std::shared_ptr<BaseUIStaticText> staticText(new UIStaticText(this, id, rectangle, text, border, background));
	staticText->SetParent(parent ? parent : mRoot);
	staticText->SetWordWrap(wordWrap);
	return staticText;
}


//! adds a window. The returned pointer must not be dropped.
std::shared_ptr<BaseUIWindow> BaseUI::AddWindow(const RectangleShape<2, int>& rectangle, bool modal,
	const wchar_t* text, const std::shared_ptr<BaseUIElement>& parent, int id)
{
	std::shared_ptr<BaseUIWindow> win(new UIWindow(this, id, rectangle));
	win->SetParent(parent ? parent : mRoot);
	win->OnInit();
	if (text)
		win->SetText(text);

	return win;
}


//! adds a scrollbar. The returned pointer must not be dropped.
std::shared_ptr<BaseUIScrollBar> BaseUI::AddScrollBar(bool horizontal, bool autoScale, 
    const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent, int id)
{
	std::shared_ptr<BaseUIScrollBar> bar(new UIScrollBar(this, id, rectangle, horizontal, autoScale));
	bar->SetParent(parent ? parent : mRoot);
	bar->OnInit();
	return bar;
}


//! adds a scrollcontainer. The returned pointer must not be dropped.
std::shared_ptr<BaseUIScrollContainer> BaseUI::AddScrollContainer(const std::string &orientation, float scrollfactor,
    const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent, int id)
{
    std::shared_ptr<BaseUIScrollContainer> scrollContainer(new UIScrollContainer(this, id, rectangle, orientation, scrollfactor));
    scrollContainer->SetParent(parent ? parent : mRoot);
    return scrollContainer;
}


//! adds a form menu. The returned pointer must not be dropped.
std::shared_ptr<BaseUIForm> BaseUI::AddForm(
    std::shared_ptr<BaseFormSource> formSrc, std::shared_ptr<TextDestination> txtDest, const std::string& formPrepend,
    const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent, int id)
{
    std::shared_ptr<BaseUIForm> formMenu(new UIForm(this, id, rectangle, formSrc, txtDest, formPrepend));
    formMenu->SetParent(parent ? parent : mRoot);
    formMenu->OnInit();
    return formMenu;
}


//! Adds a tab control to the environment.
std::shared_ptr<BaseUITabControl> BaseUI::AddTabControl(const RectangleShape<2, int>& rectangle,
    const std::shared_ptr<BaseUIElement>& parent, bool fillbackground, bool border, int id)
{
    std::shared_ptr<BaseUITabControl> table(new UITabControl(this, rectangle, fillbackground, border, id));
    table->SetParent(parent ? parent : mRoot);
    table->OnInit();
    return table;
}


//! adds a hypertext. The returned pointer must not be dropped.
std::shared_ptr<BaseUIHyperText> BaseUI::AddHypertext(const RectangleShape<2, int>& rectangle,
    const std::shared_ptr<BaseUIElement>& parent, int id, const wchar_t* text)
{
    std::shared_ptr<BaseUIHyperText> hypertext(new UIHyperText(this, id, rectangle, text));
    hypertext->SetParent(parent ? parent : mRoot);
    return hypertext;
}


//! Adds a table to the environment
std::shared_ptr<BaseUITable> BaseUI::AddTable(float scaling, 
    const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent, int id)
{
    std::shared_ptr<BaseUITable> table(new UITable(this, id, rectangle));
    table->SetParent(parent ? parent : mRoot);
    table->SetScaling(scaling);
    table->OnInit();
    return table;
}


//! Adds an image element.
std::shared_ptr<BaseUIImage> BaseUI::AddImage(std::shared_ptr<Texture2> texture, Vector2<int> pos,
	const std::shared_ptr<BaseUIElement>& parent, int id, const wchar_t* text, bool useAlphaChannel)
{
	Vector2<int> size;
	if (texture)
	{
		size[0] = texture->GetDimension(0);
		size[1] = texture->GetDimension(1);
	}

	RectangleShape<2, int> rectangle;
	rectangle.mCenter[0] = pos[0] + (size[0] / 2);
	rectangle.mCenter[1] = pos[1] + (size[1] / 2);
	rectangle.mExtent[0] = size[0];
	rectangle.mExtent[1] = size[1];
	std::shared_ptr<BaseUIImage> img(new UIImage(this, id, rectangle));
	img->SetParent(parent ? parent : mRoot);

	if (text)
		img->SetText(text);

	if (useAlphaChannel)
		img->SetUseAlphaChannel(true);

	if (texture)
		img->SetTexture(texture);

	return img;
}


//! adds an image. The returned pointer must not be dropped.
std::shared_ptr<BaseUIImage> BaseUI::AddImage(const RectangleShape<2, int>& rectangle, 
	const std::shared_ptr<BaseUIElement>& parent, int id, const wchar_t* text, bool useAlphaChannel)
{
	std::shared_ptr<BaseUIImage> img(new UIImage(this, id, rectangle));
	img->SetParent(parent ? parent : mRoot);

	if (text)
		img->SetText(text);

	if (useAlphaChannel)
		img->SetUseAlphaChannel(true);

	return img;
}

//! adds an animated image. The returned pointer must not be dropped.
std::shared_ptr<BaseUIAnimatedImage> BaseUI::AddAnimatedImage(const RectangleShape<2, int>& rectangle,
    const std::wstring &textureName, int frameCount, int frameDuration,
    const std::shared_ptr<BaseUIElement>& parent, int id)
{
    std::shared_ptr<BaseUIAnimatedImage> img(
        new UIAnimatedImage(this, id, rectangle, textureName, frameCount, frameDuration));
    img->SetParent(parent ? parent : mRoot);
    return img;
}


//! adds a checkbox
std::shared_ptr<BaseUICheckBox> BaseUI::AddCheckBox(bool checked, const RectangleShape<2, int>& rectangle, 
	const std::shared_ptr<BaseUIElement>& parent, int id, const wchar_t* text)
{
	std::shared_ptr<BaseUICheckBox> check(new UICheckBox(this, id, rectangle, checked));
	check->SetParent(parent ? parent : mRoot);
	check->OnInit();

	if (text)
		check->SetText(text);

	return check;
}


//! adds a list box
std::shared_ptr<BaseUIListBox> BaseUI::AddListBox(const RectangleShape<2, int>& rectangle,
	const std::shared_ptr<BaseUIElement>& parent, int id, bool drawBackground)
{
	std::shared_ptr<BaseUIListBox> listBox(
		new UIListBox(this, id, rectangle, true, drawBackground, false));
	listBox->SetParent(parent ? parent : mRoot);
	listBox->OnInit();

	if (mCurrentSkin && mCurrentSkin->GetSpriteBank())
	{
		listBox->SetSpriteBank(mCurrentSkin->GetSpriteBank());
	}
	else if (GetBuiltInFont() && GetBuiltInFont()->GetType() == FT_BITMAP)
	{
		listBox->SetSpriteBank(((BaseUIFontBitmap*)GetBuiltInFont().get())->GetSpriteBank());
	}

	return listBox;
}

//! adds a tree view
std::shared_ptr<BaseUITreeView> BaseUI::AddTreeView(
    const RectangleShape<2, int>& rectangle, const std::shared_ptr<BaseUIElement>& parent, 
    int id, bool drawBackground, bool scrollBarVertical, bool scrollBarHorizontal)
{
	std::shared_ptr<BaseUITreeView> treeView(new UITreeView(this, id, rectangle, true, drawBackground));
	treeView->SetParent(parent ? parent : mRoot);
	treeView->OnInit(scrollBarVertical, scrollBarHorizontal);

	treeView->SetIconFont(GetBuiltInFont());
	return treeView;
}

//! Adds an edit box. The returned pointer must not be dropped.
std::shared_ptr<BaseUIEditBox> BaseUI::AddEditBox(const wchar_t* text, 
    const RectangleShape<2, int>& rectangle, bool border, bool isEditable,
	const std::shared_ptr<BaseUIElement>& parent, int id)
{
	std::shared_ptr<BaseUIEditBox> editBox(new UIEditBox(this, id, rectangle, text, border, isEditable));
	editBox->SetParent(parent ? parent : mRoot);
	editBox->OnInit();

	return editBox;
}

//! Adds a combo box to the environment.
std::shared_ptr<BaseUIComboBox> BaseUI::AddComboBox(const RectangleShape<2, int>& rectangle,
	const std::shared_ptr<BaseUIElement>& parent, int id)
{
	std::shared_ptr<BaseUIComboBox> comboBox(new UIComboBox(this, id, rectangle));
	comboBox->SetParent(parent ? parent : mRoot);
	comboBox->OnInit();

	return comboBox;
}

void BaseUI::SetTitleText(const std::wstring& text)
{
    mTitleText->SetText(text.c_str());

    UpdateTitleTextSize();
}

void BaseUI::UpdateTitleTextSize()
{
    //create title text
    RectangleShape<2, int> rect;
    rect.mExtent = 
        GetSkin()->GetFont(DF_WINDOW)->GetDimension(mTitleText->GetText());
    rect.mCenter = rect.mExtent / 2;
    rect.mCenter += Vector2<int>{4, 0};

    mTitleText->Remove();
    mTitleText = AddStaticText(mTitleText->GetText(), rect);
}

bool BaseUI::DrawBackground()
{
    Renderer* renderer = Renderer::Get();
    Vector2<int> screenSize{
        (int)renderer->GetScreenSize()[0], (int)renderer->GetScreenSize()[1] };

    std::shared_ptr<Texture2> texture = mTextureLayer[UITL_BACKGROUND].mTexture;
    /* If no texture, draw background of solid color */
    if (!texture) 
    {
        /*
        SColor color(255, 80, 58, 37);
        RectangleShape<2, int> rect;
        rect.mExtent = Vector2<int>{ (int)screenSize[0], (int)screenSize[1] };
        rect.mCenter = rect.mExtent / 2;
        GetSkin()->Draw2DRectangle(color, mVisual, rect, &rect);
        */
        return false;
    }

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
    effect->SetTexture(texture);

    Vector2<unsigned int> sourceSize = 
        Vector2<unsigned int>{ texture->GetDimension(0), texture->GetDimension(1) };
    if (mTextureLayer[UITL_BACKGROUND].mTile)
    {
        Vector2<unsigned int> tileSize{
            std::max(sourceSize[0], mTextureLayer[UITL_BACKGROUND].mMinSize),
            std::max(sourceSize[1], mTextureLayer[UITL_BACKGROUND].mMinSize) };

        for (unsigned int x = 0; x < (unsigned int)screenSize[0]; x += tileSize[0])
        {
            for (unsigned int y = 0; y < (unsigned int)screenSize[1]; y += tileSize[1])
            {
                RectangleShape<2, int> rect;
                rect.mExtent[0] = tileSize[0];
                rect.mExtent[1] = tileSize[1];
                rect.mCenter[0] = x + rect.mExtent[0] / 2;
                rect.mCenter[1] = y + rect.mExtent[1] / 2;

                RectangleShape<2, int> tcoordRect;
                tcoordRect.mExtent[0] = sourceSize[0];
                tcoordRect.mExtent[1] = sourceSize[1];
                tcoordRect.mCenter = tcoordRect.mExtent / 2;
                GetSkin()->Draw2DTextureFilterScaled(mVisualLayout, rect, tcoordRect, colors);
            }
        }
        return true;
    }

    /* Draw background texture */
    RectangleShape<2, int> rect;
    rect.mExtent[0] = screenSize[0];
    rect.mExtent[1] = screenSize[1];
    rect.mCenter = rect.mExtent / 2;

    RectangleShape<2, int> tcoordRect;
    tcoordRect.mExtent[0] = sourceSize[0];
    tcoordRect.mExtent[1] = sourceSize[1];
    tcoordRect.mCenter = tcoordRect.mExtent / 2;
    GetSkin()->Draw2DTextureFilterScaled(mVisualLayout, rect, tcoordRect, colors);
    return true;
}

bool BaseUI::DrawOverlay()
{
    Renderer* renderer = Renderer::Get();
    Vector2<int> screenSize{
        (int)renderer->GetScreenSize()[0], (int)renderer->GetScreenSize()[1] };

    std::shared_ptr<Texture2> texture = mTextureLayer[UITL_OVERLAY].mTexture;

    /* If no texture, draw nothing */
    if (!texture)
        return false;

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
    effect->SetTexture(texture);

    Vector2<unsigned int> sourceSize =
        Vector2<unsigned int>{ texture->GetDimension(0), texture->GetDimension(1) };

    /* Draw background texture */
    RectangleShape<2, int> rect;
    rect.mExtent[0] = screenSize[0];
    rect.mExtent[1] = screenSize[1];
    rect.mCenter = rect.mExtent / 2;

    RectangleShape<2, int> tcoordRect;
    tcoordRect.mExtent[0] = sourceSize[0];
    tcoordRect.mExtent[1] = sourceSize[1];
    tcoordRect.mCenter = tcoordRect.mExtent / 2;
    GetSkin()->Draw2DTextureFilterScaled(mVisualLayout, rect, tcoordRect, colors);
    return true;
}

bool BaseUI::DrawHeader()
{
    Renderer* renderer = Renderer::Get();
    Vector2<int> screenSize{
        (int)renderer->GetScreenSize()[0], (int)renderer->GetScreenSize()[1] };

    std::shared_ptr<Texture2> texture = mTextureLayer[UITL_HEADER].mTexture;

    /* If no texture, draw nothing */
    if (!texture)
        return false;

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
    effect->SetTexture(texture);

    float mult = ((float)screenSize[0] / 2.f) / (float)texture->GetDimension(0);

    Vector2<int> splashSize = Vector2<int>{ 
        (int)(texture->GetDimension(0) * mult),
        (int)(texture->GetDimension(1) * mult) };

    Vector2<unsigned int> sourceSize =
        Vector2<unsigned int>{ texture->GetDimension(0), texture->GetDimension(1) };

    // Don't draw the header if there isn't enough room
    int freeSpace = (((int)screenSize[1]) - 320) / 2;
    if (freeSpace > splashSize[1]) 
    {
        RectangleShape<2, int> splashRect;
        splashRect.mExtent[0] = splashSize[0];
        splashRect.mExtent[1] = splashSize[1];
        splashRect.mCenter = splashRect.mExtent / 2;
        splashRect.mCenter += Vector2<int>{
            (screenSize[0] / 2) - (splashSize[0] / 2),
            ((freeSpace / 2) - splashSize[1] / 2) + 10};

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent[0] = sourceSize[0];
        tcoordRect.mExtent[1] = sourceSize[1];
        tcoordRect.mCenter = tcoordRect.mExtent / 2;

        GetSkin()->Draw2DTextureFilterScaled(mVisualLayout, splashRect, tcoordRect, colors);
        return true;
    }
    return false;
}

bool BaseUI::DrawFooter()
{
    Renderer* renderer = Renderer::Get();
    Vector2<int> screenSize{
        (int)renderer->GetScreenSize()[0], (int)renderer->GetScreenSize()[1] };

    std::shared_ptr<Texture2> texture = mTextureLayer[UITL_FOOTER].mTexture;

    /* If no texture, draw nothing */
    if (!texture)
        return false;

    const SColor color(255, 255, 255, 255);
    const SColor colors[] = { color, color, color, color };

    auto effect = std::dynamic_pointer_cast<Texture2Effect>(mEffect);
    effect->SetTexture(texture);

    float mult = (float)screenSize[0] / (float)texture->GetDimension(0);

    Vector2<int> footerSize = Vector2<int>{
        (int)(texture->GetDimension(0) * mult),
        (int)(texture->GetDimension(1) * mult) };

    Vector2<unsigned int> sourceSize =
        Vector2<unsigned int>{ texture->GetDimension(0), texture->GetDimension(1) };

    // Don't draw the footer if there isn't enough room
    int freeSpace = (((int)screenSize[1]) - 320) / 2;
    if (freeSpace > footerSize[1])
    {
        RectangleShape<2, int> footerRect;
        footerRect.mExtent[0] = footerSize[0];
        footerRect.mExtent[1] = footerSize[1];
        footerRect.mCenter = footerRect.mExtent / 2;
        footerRect.mCenter += Vector2<int>{screenSize[0] / 2, screenSize[1] - footerSize[1]};
        footerRect.mCenter -= Vector2<int>{footerSize[0] / 2, 0};

        RectangleShape<2, int> tcoordRect;
        tcoordRect.mExtent[0] = sourceSize[0];
        tcoordRect.mExtent[1] = sourceSize[1];
        tcoordRect.mCenter = tcoordRect.mExtent / 2;

        GetSkin()->Draw2DTextureFilterScaled(mVisualLayout, footerRect, tcoordRect, colors);
        return true;
    }
    return false;
}

bool BaseUI::SetTexture(const std::wstring& identifier, 
    const std::wstring& texturePath, bool tileTexture, unsigned int minSize)
{
    UITextureLayer layer;
    if (identifier == L"background")
        layer = UITL_BACKGROUND;
    else if (identifier == L"overlay")
        layer = UITL_OVERLAY;
    else if (identifier == L"header")
        layer = UITL_HEADER;
    else if (identifier == L"footer")
        layer = UITL_FOOTER;
    else
        return false;

    if (mTextureLayer[layer].mTexture) 
        mTextureLayer[layer].mTexture = nullptr;

    if (texturePath.empty())
        return false;

    std::shared_ptr<ResHandle>& resHandle = 
        ResCache::Get()->GetHandle(&BaseResource(texturePath));
    if (resHandle)
    {
        const std::shared_ptr<ImageResourceExtraData>& extra =
            std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
        extra->GetImage()->AutogenerateMipmaps();
        mTextureLayer[layer].mTexture = extra->GetImage();
        mTextureLayer[layer].mTile = tileTexture;
        mTextureLayer[layer].mMinSize = minSize;
    }

    if (!mTextureLayer[layer].mTexture)
        return false;

    return true;
}