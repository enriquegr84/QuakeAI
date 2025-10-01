// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UISkin.h"

#include "Application/Settings.h"

#include "Graphic/Image/ImageFilter.h"
#include "Graphic/Renderer/Renderer.h"


UISkin::UISkin(BaseUI* ui, UISkinThemeType type)
	: mSpriteBank(0), mType(type), mUI(ui)
{
	if ((mType == STT_WINDOWS_CLASSIC) || (mType == STT_WINDOWS_METALLIC))
	{
        mColors[DC_3D_DARK_SHADOW] = SColor(101, 50, 50, 50);
        mColors[DC_3D_SHADOW] = SColor(101, 130, 130, 130);
        mColors[DC_3D_FACE] = SColor(220, 100, 100, 100);
        mColors[DC_3D_HIGH_LIGHT] = SColor(101, 255, 255, 255);
        mColors[DC_3D_LIGHT] = SColor(101, 210, 210, 210);
        mColors[DC_ACTIVE_BORDER] = SColor(101, 16, 14, 115);
        mColors[DC_ACTIVE_CAPTION] = SColor(255, 255, 255, 255);
        mColors[DC_APP_WORKSPACE] = SColor(101, 100, 100, 100);
        mColors[DC_BUTTON_TEXT] = SColor(240, 10, 10, 10);
        mColors[DC_GRAY_TEXT] = SColor(240, 130, 130, 130);
        mColors[DC_HIGH_LIGHT] = SColor(101, 8, 36, 107);
        mColors[DC_HIGH_LIGHT_TEXT] = SColor(240, 255, 255, 255);
        mColors[DC_INACTIVE_BORDER] = SColor(101, 165, 165, 165);
        mColors[DC_INACTIVE_CAPTION] = SColor(255, 30, 30, 30);
        mColors[DC_TOOLTIP] = SColor(200, 0, 0, 0);
        mColors[DC_TOOLTIP_BACKGROUND] = SColor(200, 255, 255, 225);
        mColors[DC_SCROLLBAR] = SColor(101, 230, 230, 230);
        mColors[DC_WINDOW] = SColor(101, 255, 255, 255);
        mColors[DC_WINDOW_SYMBOL] = SColor(200, 10, 10, 10);
        mColors[DC_ICON_NORMAL] = SColor(200, 255, 255, 255);
        mColors[DC_ICON_HIGH_LIGHT] = SColor(200, 8, 36, 107);
        mColors[DC_GRAY_WINDOW_SYMBOL] = SColor(240, 100, 100, 100);
        mColors[DC_EDITABLE] = SColor(255, 255, 255, 255);
        mColors[DC_GRAY_EDITABLE] = SColor(255, 120, 120, 120);
        mColors[DC_FOCUSED_EDITABLE] = SColor(255, 240, 240, 255);

		mSizes[DS_SCROLLBAR_SIZE] = 14;
		mSizes[DS_MENU_HEIGHT] = 30;
		mSizes[DS_WINDOW_BUTTON_WIDTH] = 15;
		mSizes[DS_CHECK_BOX_WIDTH] = 20;
		mSizes[DS_MESSAGE_BOX_WIDTH] = 500;
		mSizes[DS_MESSAGE_BOX_HEIGHT] = 200;
		mSizes[DS_BUTTON_WIDTH] = 80;
		mSizes[DS_BUTTON_HEIGHT] = 30;

		mSizes[DS_TEXT_DISTANCE_X] = 2;
		mSizes[DS_TEXT_DISTANCE_Y] = 0;

		mSizes[DS_TITLEBARTEXT_DISTANCE_X] = 2;
		mSizes[DS_TITLEBARTEXT_DISTANCE_Y] = 0;
	}
	else
	{
        //0x80a6a8af
        mColors[DC_3D_DARK_SHADOW] = 0x60767982;
        //Colors[DC_3D_FACE]			=	0xc0c9ccd4;		// tab background
        mColors[DC_3D_FACE] = 0xc0cbd2d9;		// tab background
        mColors[DC_3D_SHADOW] = 0x50e4e8f1;		// tab background, and left-top highlight
        mColors[DC_3D_HIGH_LIGHT] = 0x40c7ccdc;
        mColors[DC_3D_LIGHT] = 0x802e313a;
        mColors[DC_ACTIVE_BORDER] = 0x80404040;		// window title
        mColors[DC_ACTIVE_CAPTION] = 0xffd0d0d0;
        mColors[DC_APP_WORKSPACE] = 0xc0646464;		// unused
        mColors[DC_BUTTON_TEXT] = 0xd0161616;
        mColors[DC_GRAY_TEXT] = 0x3c141414;
        mColors[DC_HIGH_LIGHT] = 0x6c606060;
        mColors[DC_HIGH_LIGHT_TEXT] = 0xd0e0e0e0;
        mColors[DC_INACTIVE_BORDER] = 0xf0a5a5a5;
        mColors[DC_INACTIVE_CAPTION] = 0xffd2d2d2;
        mColors[DC_TOOLTIP] = 0xf00f2033;
        mColors[DC_TOOLTIP_BACKGROUND] = 0xc0cbd2d9;
        mColors[DC_SCROLLBAR] = 0xf0e0e0e0;
        mColors[DC_WINDOW] = 0xf0f0f0f0;
        mColors[DC_WINDOW_SYMBOL] = 0xd0161616;
        mColors[DC_ICON_NORMAL] = 0xd0161616;
        mColors[DC_ICON_HIGH_LIGHT] = 0xd0606060;
        mColors[DC_GRAY_WINDOW_SYMBOL] = 0x3c101010;
        mColors[DC_EDITABLE] = 0xf0ffffff;
        mColors[DC_GRAY_EDITABLE] = 0xf0cccccc;
        mColors[DC_FOCUSED_EDITABLE] = 0xf0fffff0;

		mSizes[DS_SCROLLBAR_SIZE] = 14;
		mSizes[DS_MENU_HEIGHT] = 48;
		mSizes[DS_WINDOW_BUTTON_WIDTH] = 15;
		mSizes[DS_CHECK_BOX_WIDTH] = 20;
		mSizes[DS_MESSAGE_BOX_WIDTH] = 500;
		mSizes[DS_MESSAGE_BOX_HEIGHT] = 200;
		mSizes[DS_BUTTON_WIDTH] = 80;
		mSizes[DS_BUTTON_HEIGHT] = 30;

		mSizes[DS_TEXT_DISTANCE_X] = 3;
		mSizes[DS_TEXT_DISTANCE_Y] = 2;

		mSizes[DS_TITLEBARTEXT_DISTANCE_X] = 3;
		mSizes[DS_TITLEBARTEXT_DISTANCE_Y] = 2;
	}

	mSizes[DS_MESSAGE_BOX_GAP_SPACE] = 15;
	mSizes[DS_MESSAGE_BOX_MIN_TEXT_WIDTH] = 0;
	mSizes[DS_MESSAGE_BOX_MAX_TEXT_WIDTH] = 500;
	mSizes[DS_MESSAGE_BOX_MIN_TEXT_HEIGHT] = 0;
	mSizes[DS_MESSAGE_BOX_MAX_TEXT_HEIGHT] = 99999;

	mSizes[DS_BUTTON_PRESSED_IMAGE_OFFSET_X] = 1;
	mSizes[DS_BUTTON_PRESSED_IMAGE_OFFSET_Y] = 1;
	mSizes[DS_BUTTON_PRESSED_TEXT_OFFSET_X] = 0;
	mSizes[DS_BUTTON_PRESSED_TEXT_OFFSET_Y] = 2;

	mTexts[DT_MSG_BOX_OK] = L"OK";
	mTexts[DT_MSG_BOX_CANCEL] = L"Cancel";
	mTexts[DT_MSG_BOX_YES] = L"Yes";
	mTexts[DT_MSG_BOX_NO] = L"No";
	mTexts[DT_WINDOW_CLOSE] = L"Close";
	mTexts[DT_WINDOW_RESTORE] = L"Restore";
	mTexts[DT_WINDOW_MINIMIZE] = L"Minimize";
	mTexts[DT_WINDOW_MAXIMIZE] = L"Maximize";
	mTexts[DT_WINDOW_COLLAPSE] = L"Collapse";
	mTexts[DT_WINDOW_EXPAND] = L"Expand";

	mIcons[DI_WINDOW_MAXIMIZE] = L"Art/UserControl/appbar.window.maximize.png";
	mIcons[DI_WINDOW_RESTORE] = L"Art/UserControl/appbar.window.restore.png";
	mIcons[DI_WINDOW_CLOSE] = L"Art/UserControl/appbar.close.png";
	mIcons[DI_WINDOW_MINIMIZE] = L"Art/UserControl/appbar.window.minimize.png";
	mIcons[DI_WINDOW_RESIZE] = L"Art/UserControl/appbar.window.restore.png";
	mIcons[DI_WINDOW_COLLAPSE] = L"Art/UserControl/appbar.arrow.collapsed.png";
	mIcons[DI_WINDOW_EXPAND] = L"Art/UserControl/appbar.arrow.expand.png";
	mIcons[DI_CURSOR_UP] = L"Art/UserControl/appbar.chevron.up.png";
	mIcons[DI_CURSOR_DOWN] = L"Art/UserControl/appbar.chevron.down.png";
	mIcons[DI_CURSOR_LEFT] = L"Art/UserControl/appbar.chevron.left.png";
	mIcons[DI_CURSOR_RIGHT] = L"Art/UserControl/appbar.chevron.right.png";
	mIcons[DI_MENU_MORE] = L"Art/UserControl/appbar.add.png";
	mIcons[DI_CHECKBOX_CHECKED] = L"Art/UserControl/appbar.checkbox.check.png";
	mIcons[DI_CHECKBOX_UNCHECKED] = L"Art/UserControl/appbar.checkbox.uncheck.png";
	mIcons[DI_DROP_DOWN] = L"Art/UserControl/appbar.chevron.down.png";
	mIcons[DI_SMALL_CURSOR_UP] = L"Art/UserControl/appbar.cursor.pointer.png";
	mIcons[DI_SMALL_CURSOR_DOWN] = L"Art/UserControl/appbar.cursor.pointer.png";
	mIcons[DI_RADIO_BUTTON_CHECKED] = L"Art/UserControl/appbar.checkmark.cross.png";
	mIcons[DI_MORE_LEFT] = L"Art/UserControl/appbar.chevron.left.png";
	mIcons[DI_MORE_RIGHT] = L"Art/UserControl/appbar.chevron.right.png";
	mIcons[DI_MORE_UP] = L"Art/UserControl/appbar.chevron.up.png";
	mIcons[DI_MORE_DOWN] = L"Art/UserControl/appbar.chevron.down.png";
	mIcons[DI_EXPAND] = L"Art/UserControl/appbar.arrow.expand.png";
	mIcons[DI_COLLAPSE] = L"Art/UserControl/appbar.arrow.collapsed.png";

	mIcons[DI_FILE] = L"Art/UserControl/appbar.file.png";
	mIcons[DI_DIRECTORY] = L"Art/UserControl/appbar.folder.png";

	for (unsigned int i=0; i<DF_COUNT; ++i)
		mFonts[i] = 0;

	mUseGradient = mType == STT_WINDOWS_METALLIC;
}


//! destructor
UISkin::~UISkin()
{

}

//! returns default color
SColor UISkin::GetColor(UIDefaultColor color) const
{
	if ((unsigned int)color < DC_COUNT)
		return mColors[color];
	else
		return SColor();
}


//! sets a default color
void UISkin::SetColor(UIDefaultColor which, SColor newColor)
{
	if ((unsigned int)which < DC_COUNT)
		mColors[which] = newColor;
}


//! returns size for the given size type
int UISkin::GetSize(UIDefaultSize size) const
{
	if ((unsigned int)size < DS_COUNT)
		return mSizes[size];
	else
		return 0;
}


//! sets a default size
void UISkin::SetSize(UIDefaultSize which, int size)
{
	if ((unsigned int)which < DS_COUNT)
		mSizes[which] = size;
}


//! returns the default font
const std::shared_ptr<BaseUIFont>& UISkin::GetFont(UIDefaultFont which) const
{
	if (((unsigned int)which < DF_COUNT) && mFonts[which])
		return mFonts[which];
	else
		return mFonts[DF_DEFAULT];
}


//! sets a default font
void UISkin::SetFont(const std::shared_ptr<BaseUIFont>& font, UIDefaultFont which)
{
	if ((unsigned int)which >= DF_COUNT)
		return;

	if (font)
		mFonts[which] = font;
}


//! gets the sprite bank stored
const std::shared_ptr<BaseUISpriteBank>& UISkin::GetSpriteBank() const
{
	return mSpriteBank;
}


//! set a new sprite bank or Remove one by passing 0
void UISkin::SetSpriteBank(const std::shared_ptr<BaseUISpriteBank>& bank)
{
	mSpriteBank = bank;
}


//! Returns a default icon
const wchar_t* UISkin::GetIcon(UIDefaultIcon icon) const
{
	if ((unsigned int)icon < DI_COUNT)
		return mIcons[icon].c_str();
	else
		return nullptr;
}


//! Sets a default icon
void UISkin::SetIcon(UIDefaultIcon icon, wchar_t* path)
{
	if ((unsigned int)icon < DI_COUNT)
		mIcons[icon] = path;
}


//! Returns a default text. For example for Message box button captions:
//! "OK", "Cancel", "Yes", "No" and so on.
const wchar_t* UISkin::GetDefaultText(UIDefaultText text) const
{
	if ((unsigned int)text < DT_COUNT)
		return mTexts[text].c_str();
	else
		return mTexts[0].c_str();
}


//! Sets a default text. For example for Message box button captions:
//! "OK", "Cancel", "Yes", "No" and so on.
void UISkin::SetDefaultText(UIDefaultText which, const wchar_t* newText)
{
	if ((unsigned int)which < DT_COUNT)
		mTexts[which] = newText;
}


//! Manually clear the cache, e.g. when switching to different worlds.
void UISkin::ClearTextureCache()
{
    mScaledTextures.clear();
    mOriginalTextures.clear();
}

/* Manually insert an image into the cache, useful to avoid texture-to-image
 * conversion whenever we can intercept it. */
std::shared_ptr<Texture2> UISkin::AddTexture(std::shared_ptr<Texture2> srcTexture)
{
    if (!Settings::Get()->GetBool("gui_scaling_filter"))
        return srcTexture;

    // Create the 2D texture and compute the stride and image size.
    std::shared_ptr<Texture2> srcImg = std::make_shared<Texture2>(
        srcTexture->GetFormat(), srcTexture->GetWidth(), srcTexture->GetHeight(), srcTexture->HasMipmaps());

    // Copy the pixels from the decoder to the texture.
    std::memcpy(srcImg->Get<BYTE>(), srcTexture->Get<BYTE>(), srcImg->GetNumBytes());
    mOriginalTextures[srcTexture->GetName()] = srcImg;
    return srcImg;
}

/* Add a cached, high-quality pre-scaled texture for display purposes.  If the
 * texture is not already cached, attempt to create it. Returns a pre-scaled texture,
 * or the original texture if unable to pre-scale it. */
std::shared_ptr<Texture2> UISkin::AddScaledTexture(std::shared_ptr<Texture2> srcTexture,
    const RectangleShape<2, int>& posRect, const RectangleShape<2, int>& tcoordRect)
{
    if (!Settings::Get()->GetBool("gui_scaling_filter"))
        return srcTexture;

    // Calculate scaled texture name.
    wchar_t rectstr[200];
    swprintf(rectstr, L"%d:%d:%d:%d:%d:%d",
        tcoordRect.mCenter[0], tcoordRect.mCenter[1],
        tcoordRect.mExtent[0], tcoordRect.mExtent[1],
        posRect.mExtent[0], posRect.mExtent[1]);
    std::wstring scaleName = srcTexture->GetName() + L"@guiScalingFilter:" + rectstr;

    // Search for existing scaled texture.
    std::shared_ptr<Texture2> scaled = mScaledTextures[scaleName];
    if (scaled)
        return scaled;

    std::shared_ptr<Texture2> srcImg = mOriginalTextures[srcTexture->GetName()];
    if (!srcImg)
    {
        if (!Settings::Get()->GetBool("gui_scaling_filter_txr2img"))
            return srcTexture;

        // Create the 2D texture and compute the stride and image size.
        srcImg = std::make_shared<Texture2>(
            srcTexture->GetFormat(), srcTexture->GetWidth(), srcTexture->GetHeight(), srcTexture->HasMipmaps());

        // Copy the pixels from the decoder to the texture.
        std::memcpy(srcImg->Get<BYTE>(), srcTexture->Get<BYTE>(), srcImg->GetNumBytes());
        mOriginalTextures[srcTexture->GetName()] = srcImg;
    }

    // Create a new destination image and scale the source into it.
    ImageFilter::ImageCleanTransparent(srcImg, 0);
    std::shared_ptr<Texture2> destImg = std::make_shared<Texture2>(
        srcTexture->GetFormat(), posRect.mExtent[0], posRect.mExtent[1], srcTexture->HasMipmaps());
    ImageFilter::ImageScaleNNAA(srcImg, tcoordRect, destImg);

    // Convert the scaled image back into a texture.
    scaled = destImg;
    mScaledTextures[scaleName] = scaled;
    return scaled;
}


//! draws a standard 3d button pane
void UISkin::Draw3DButtonPaneStandard(const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

    RectangleShape<2, int> rect = frameRect;
    rect.mCenter[0] += 1;
    rect.mCenter[1] += 1;
    Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);

    rect.mCenter[0] -= 1;
    rect.mCenter[1] -= 1;
    Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

    rect.mCenter[0] += 1;
    rect.mExtent[0] -= 2;
    rect.mCenter[1] += 1;
    rect.mExtent[1] -= 2;
    Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

    rect.mCenter[0] -= 1;
    rect.mCenter[1] -= 1;

    if (mUseGradient)
    {
        SColorF c1 = SColorF(colors[DC_3D_FACE]);
        SColorF c2 = SColorF(c1.GetInterpolated(colors[DC_3D_DARK_SHADOW], 0.4f));

        Draw2DRectangle(c1, c2, visual, rect, clip);
    }
    else
    {
        Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);
    }
}


//! draws a pressed 3d button pane
void UISkin::Draw3DButtonPanePressed(const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

    RectangleShape<2, int> rect = frameRect;
    Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

    rect.mCenter[0] -= 1;
    rect.mExtent[0] -= 1;
    rect.mCenter[1] -= 1;
    rect.mExtent[1] -= 1;
    Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);

    rect.mCenter[0] += 1;
    rect.mExtent[0] -= 1;
    rect.mCenter[1] += 1;
    rect.mExtent[1] -= 1;
    Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

    rect.mCenter[0] += 1;
    rect.mCenter[1] += 1;

    if (mUseGradient)
    {
        SColorF c1 = SColorF(colors[DC_3D_FACE]);
        SColorF c2 = SColorF(c1.GetInterpolated(colors[DC_3D_DARK_SHADOW], 0.4f));

        Draw2DRectangle(c1, c2, visual, rect, clip);
    }
    else
    {
        Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);
    }
}


//! draws a sunken 3d pane
void UISkin::Draw3DSunkenPane(SColorF bgcolor, bool flat, bool fillBackGround, 
    const std::shared_ptr<Visual>& visual, const RectangleShape<2, int>& frameRect, 
    const RectangleShape<2, int>* clip, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

    RectangleShape<2, int> rect = frameRect;

    if (fillBackGround)
        Draw2DRectangle(bgcolor, visual, rect, clip);

    if (flat)
    {
        // draw flat sunken pane
        rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1];
        rect.mExtent[1] = 1;
        Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);	// top

        rect = frameRect;
        rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0] - 1;
        rect.mExtent[0] = 1;
        rect.mExtent[1] += 1;
        Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);	// left

        rect = frameRect;
        rect.mCenter[0] = rect.GetVertice(RVP_LOWERRIGHT)[0];
        rect.mExtent[0] = 1;
        rect.mExtent[1] += 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);	// right

        rect = frameRect;
        rect.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1];
        rect.mExtent[1] = 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);	// bottom
    }
    else
    {
        // draw deep sunken pane
        rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1];
        rect.mExtent[1] = 1;
        Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);	// top
        rect.mExtent[0] -= 2;
        rect.mCenter[1] += 1;
        Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);

        rect = frameRect;
        rect.mCenter[0] = frameRect.GetVertice(RVP_UPPERLEFT)[0] - 1;
        rect.mExtent[0] = 1;
        rect.mExtent[1] += 1;
        Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);	// left
        rect.mCenter[0] += 1;
        rect.mExtent[1] -= 2;
        Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);

        rect = frameRect;
        rect.mCenter[0] = frameRect.GetVertice(RVP_LOWERRIGHT)[0];
        rect.mExtent[0] = 1;
        rect.mExtent[1] += 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);	// right
        rect.mCenter[0] -= 1;
        rect.mExtent[1] -= 2;
        Draw2DRectangle(colors[DC_3D_LIGHT], visual, rect, clip);

        rect = frameRect;
        rect.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1];
        rect.mExtent[1] = 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);	// bottom
        rect.mExtent[0] -= 2;
        rect.mCenter[1] -= 1;
        Draw2DRectangle(colors[DC_3D_LIGHT], visual, rect, clip);
    }
}


//! draws a window background
// return where to draw title bar text.
RectangleShape<2, int> UISkin::Draw3DWindowBackground(const std::shared_ptr<Visual>& visualBackground, 
    const std::shared_ptr<Visual>& visualTitle, bool drawTitleBar, SColor titleBarColor, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip,
    RectangleShape<2, int>* checkClientArea, const SColor* colors)
{
	if (!Renderer::Get())
	{
		if ( checkClientArea )
			*checkClientArea = frameRect;

		return frameRect;
	}

    if (!colors)
        colors = mColors;
	
	RectangleShape<2, int> rect = frameRect;

    // top border
    rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1];
    rect.mExtent[1] = 1;
    if (!checkClientArea)
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visualBackground, rect, clip);

    // left border
    rect = frameRect;
    rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0];
    rect.mExtent[0] = 1;
    if (!checkClientArea)
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visualBackground, rect, clip);

    // right border dark outer line
    rect = frameRect;
    rect.mCenter[0] = frameRect.GetVertice(RVP_UPPERLEFT)[0];
    rect.mExtent[0] = 1;
    if (!checkClientArea)
        Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visualBackground, rect, clip);

    // right border bright innner line
    rect.mCenter[0] -= 1;
    rect.mExtent[1] -= 2;
    if (!checkClientArea)
        Draw2DRectangle(colors[DC_3D_SHADOW], visualBackground, rect, clip);

    // client area for background
    rect = frameRect;
    rect.mExtent[0] -= 2;
    rect.mExtent[1] -= 2;
    if (checkClientArea)
        *checkClientArea = rect;

    if (!checkClientArea)
    {
        if (mUseGradient)
        {
            SColorF c1 = SColorF(colors[DC_3D_SHADOW]);
            SColorF c2 = SColorF(colors[DC_3D_FACE]);

            Draw2DRectangle(c1, c2, visualBackground, rect, clip);
        }
        else
        {
            Draw2DRectangle(colors[DC_3D_FACE], visualBackground, rect, clip);
        }
    }

	// title bar
	rect = frameRect;
	rect.mExtent[0] -= 2;
	rect.mExtent[1] -= 2;
	rect.mCenter[1] = (rect.mCenter[1] - rect.mExtent[1] / 2) + (GetSize(DS_WINDOW_BUTTON_WIDTH) + 2) / 2;
	rect.mExtent[1] = GetSize(DS_WINDOW_BUTTON_WIDTH) + 2;

	if (drawTitleBar)
	{
		if (!checkClientArea)
		{
            SColorF color = SColorF(
                titleBarColor.GetInterpolated(SColor(titleBarColor.GetAlpha(), 0, 0, 0), 0.2f));
            Draw2DRectangle(color, visualTitle, rect, clip);
		}
		else
		{
			(*checkClientArea).mCenter[1] += (int)round((*checkClientArea).mExtent[1] / 2.f);
			(*checkClientArea).mExtent[1] = 0;
		}
	}
	return rect;
}


//! draws a standard 3d menu pane
void UISkin::Draw3DMenuPane(const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

	RectangleShape<2, int> rect = frameRect;

    // in this skin, this is exactly what non pressed buttons look like,
    // so we could simply call Draw3DButtonPaneStandard(element, rect, clip); here.
    // but if the skin is transparent, this doesn't look that nice. So
    // We draw it a little bit better, with some more draw2DRectangle calls,
    // but there aren't that much menus visible anyway.

    // top border
    rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1];
    rect.mExtent[1] = 1;
    Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

    // left border
    rect = frameRect;
    rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0];
    rect.mExtent[0] = 1;
    Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

    // right border dark outer line
    rect = frameRect;
    rect.mCenter[0] = frameRect.GetVertice(RVP_UPPERLEFT)[0];
    rect.mExtent[0] = 1;
    Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);

    // right border bright innner line
    rect.mCenter[0] -= 1;
    rect.mExtent[1] -= 2;
    Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

    rect = frameRect;
    rect.mCenter[1] = rect.GetVertice(RVP_LOWERRIGHT)[1];
    rect.mExtent[1] = 1;
    Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);

    rect.mExtent[0] -= 2;
    rect.mCenter[1] -= 1;
    Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

    rect = frameRect;
    rect.mExtent[0] -= 2;
    rect.mExtent[1] -= 2;
    if (mUseGradient)
    {
        SColorF c1 = SColorF(colors[DC_3D_FACE]);
        SColorF c2 = SColorF(colors[DC_3D_SHADOW]);
        Draw2DRectangle(c1, c2, visual, rect, clip);
    }
    else Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);
}


//! draws a standard 3d tool bar
void UISkin::Draw3DToolBar(const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

	RectangleShape<2, int> rect = frameRect;
	
    rect.mCenter[1] = rect.GetVertice(RVP_LOWERRIGHT)[1];
    rect.mExtent[1] = 1;
    Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

    rect = frameRect;
    rect.mCenter[1] -= 1;

    if (mUseGradient)
    {
        SColorF c1 = SColorF(colors[DC_3D_FACE]);
        SColorF c2 = SColorF(colors[DC_3D_SHADOW]);
        Draw2DRectangle(c1, c2, visual, rect, clip);
    }
    else Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);
}


//! draws a tab button
void UISkin::Draw3DTabButton(bool active, const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip, 
    UIAlignment alignment, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

	RectangleShape<2, int> rect = frameRect;

    if (alignment == UIA_UPPERLEFT)
    {
        // draw grey background
        rect.mExtent[0] -= 4;
        Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);

        rect.mCenter[1] = rect.GetVertice(RVP_UPPERLEFT)[1];
        rect.mExtent[1] = 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

        // draw left highlight
        rect = frameRect;
        rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0] + 1;
        rect.mExtent[0] = 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

        // draw right middle gray shadow
        rect = frameRect;
        rect.mExtent[0] -= 4;
        rect.mCenter[0] = rect.GetVertice(RVP_LOWERRIGHT)[0];
        rect.mExtent[0] = 1;
        Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

        rect.mCenter[0] += 1;
        Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);
    }
    else
    {
        // draw grey background
        rect.mExtent[0] -= 4;
        Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);

        rect.mCenter[1] = rect.GetVertice(RVP_LOWERRIGHT)[1];
        rect.mExtent[1] = 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

        // draw left highlight
        rect = frameRect;
        rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0] + 1;
        rect.mExtent[0] = 1;
        Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

        // draw right middle gray shadow
        rect = frameRect;
        rect.mExtent[0] -= 4;
        rect.mCenter[0] = rect.GetVertice(RVP_LOWERRIGHT)[0];
        rect.mExtent[0] = 1;
        Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

        rect.mCenter[0] += 1;
        Draw2DRectangle(colors[DC_3D_DARK_SHADOW], visual, rect, clip);
    }
}


//! draws a tab control body
void UISkin::Draw3DTabBody(bool border, bool background, 
    const std::shared_ptr<Visual>& visual, const RectangleShape<2, int>& frameRect, 
    const RectangleShape<2, int>* clip, int tabHeight, UIAlignment alignment, const SColor* colors)
{
	if (!Renderer::Get())
		return;

    if (!colors)
        colors = mColors;

	RectangleShape<2, int> rect = frameRect;
	
    if (tabHeight == -1)
        tabHeight = GetSize(DS_BUTTON_HEIGHT);

    // draw border.
    if (border)
    {
        if (alignment == UIA_UPPERLEFT)
        {
            // draw left hightlight
            rect.mExtent[1] += tabHeight + 2;
            rect.mCenter[1] += (tabHeight + 2) / 2;
            rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0];
            rect.mExtent[0] = 1;
            Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

            // draw right shadow
            rect.mCenter[0] = frameRect.GetVertice(RVP_LOWERRIGHT)[0];
            rect.mExtent[0] = 1;
            Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

            // draw lower shadow
            rect = frameRect;
            rect.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1];
            rect.mExtent[1] = 1;
            Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);
        }
        else
        {
            // draw left hightlight
            rect.mExtent[1] -= tabHeight + 2;
            rect.mCenter[1] -= (tabHeight + 2) / 2;
            rect.mCenter[0] = rect.GetVertice(RVP_UPPERLEFT)[0];
            rect.mExtent[0] = 1;
            Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);

            // draw right shadow
            rect.mCenter[0] = frameRect.GetVertice(RVP_LOWERRIGHT)[0];
            rect.mExtent[0] = 1;
            Draw2DRectangle(colors[DC_3D_SHADOW], visual, rect, clip);

            // draw lower shadow
            rect = frameRect;
            rect.mCenter[1] = frameRect.GetVertice(RVP_LOWERRIGHT)[1];
            rect.mExtent[1] = 1;
            Draw2DRectangle(colors[DC_3D_HIGH_LIGHT], visual, rect, clip);
        }
    }

    if (background)
    {
        if (alignment == UIA_UPPERLEFT)
        {
            rect = frameRect;
            rect.mExtent[1] += tabHeight + 2;
            rect.mCenter[1] += (tabHeight + 2) / 2;
            rect.mExtent[0] += 2;
        }
        else
        {
            rect = frameRect;
            rect.mExtent[1] -= tabHeight + 2;
            rect.mCenter[1] -= (tabHeight + 2) / 2;
            rect.mExtent[0] -= 2;
        }

        if (mUseGradient)
        {
            SColorF c1 = SColorF(colors[DC_3D_FACE]);
            SColorF c2 = SColorF(colors[DC_3D_SHADOW]);
            Draw2DRectangle(c1, c2, visual, rect, clip);
        }
        else Draw2DRectangle(colors[DC_3D_FACE], visual, rect, clip);
    }
}


//! draws an icon, usually from the skin's sprite bank
void UISkin::DrawIcon(const std::shared_ptr<BaseUIElement>& element, UIDefaultIcon icon,
    const std::shared_ptr<Visual>& visual, const RectangleShape<2, int> destRect,
    const RectangleShape<2, int>* clip, unsigned int startTime, unsigned int currentTime, 
    bool loop, const SColor* colors, bool center)
{
    if (!mSpriteBank)
        return;

    if (!colors)
        colors = mColors;

    bool gray = element && !element->IsEnabled();

    mSpriteBank->Draw2DSprite(icon, visual, destRect, clip,
        colors[gray ? DC_GRAY_WINDOW_SYMBOL : DC_WINDOW_SYMBOL], startTime, currentTime, loop, center);
}

//! draws a 2d texture.
void UISkin::Draw2DTexture(const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& posRect, const SColor* colors, const RectangleShape<2, int>* clip)
{
    std::shared_ptr<Texture2Effect> effect =
        std::static_pointer_cast<Texture2Effect>(visual->GetEffect());
    std::shared_ptr<Texture2> tex = effect->GetTexture();
    RectangleShape<2, int> tcoordRect;
    tcoordRect.mCenter = Vector2<int>{(int)tex->GetDimension(0) / 2, (int)tex->GetDimension(1) / 2 };
    tcoordRect.mExtent = Vector2<int>{(int)tex->GetDimension(0), (int)tex->GetDimension(1)};

    Draw2DTexture(visual, posRect, tcoordRect, colors, clip);
}

//! draws a 2d texture.
void UISkin::Draw2DTexture(const std::shared_ptr<Visual>& visual, const RectangleShape<2, int>& posRect, 
    const RectangleShape<2, int>& tcoordRect, const SColor* colors, const RectangleShape<2, int>* clip)
{
    if (!Renderer::Get())
        return;

    // clip these coordinates
    RectangleShape<2, int> targetRect(posRect);
    if (clip)
    {
        targetRect.ClipAgainst(*clip);
        if (targetRect.mExtent[0] < 0 || targetRect.mExtent[1] < 0)
            return;
    }

    const SColor temp[4] =
    {
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF
    };
    const SColor* const useColor = colors ? colors : temp;

    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
    Vector2<int> dimension{ (int)screenSize[0] / 2, (int)screenSize[1] / 2 };

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
    };
    Vertex* vertex = visual->GetVertexBuffer()->Get<Vertex>();
    vertex[0].position = {
        (float)(targetRect.mCenter[0] - dimension[0] - (targetRect.mExtent[0] / 2)) / dimension[0],
        (float)(dimension[1] - targetRect.mCenter[1] - (targetRect.mExtent[1] / 2)) / dimension[1], 0.0f };
    vertex[0].tcoord = {
        (float)(tcoordRect.mCenter[0] - (tcoordRect.mExtent[0] / 2)) / tcoordRect.mExtent[0],
        (float)(tcoordRect.mCenter[1] + (int)round(tcoordRect.mExtent[1] / 2.f)) / tcoordRect.mExtent[1] };
    vertex[0].color = SColorF(useColor[0]).ToArray();

    vertex[1].position = {
        (float)(targetRect.mCenter[0] - dimension[0] + (int)round(targetRect.mExtent[0] / 2.f)) / dimension[0],
        (float)(dimension[1] - targetRect.mCenter[1] - (targetRect.mExtent[1] / 2)) / dimension[1], 0.0f };
    vertex[1].tcoord = {
        (float)(tcoordRect.mCenter[0] + (int)round(tcoordRect.mExtent[0] / 2.f)) / tcoordRect.mExtent[0],
        (float)(tcoordRect.mCenter[1] + (int)round(tcoordRect.mExtent[1] / 2.f)) / tcoordRect.mExtent[1] };
    vertex[1].color = SColorF(useColor[3]).ToArray();

    vertex[2].position = {
        (float)(targetRect.mCenter[0] - dimension[0] - (targetRect.mExtent[0] / 2)) / dimension[0],
        (float)(dimension[1] - targetRect.mCenter[1] + (int)round(targetRect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
    vertex[2].tcoord = {
        (float)(tcoordRect.mCenter[0] - (tcoordRect.mExtent[0] / 2)) / tcoordRect.mExtent[0],
        (float)(tcoordRect.mCenter[1] - (tcoordRect.mExtent[1] / 2)) / tcoordRect.mExtent[1] };
    vertex[2].color = SColorF(useColor[1]).ToArray();

    vertex[3].position = {
        (float)(targetRect.mCenter[0] - dimension[0] + (int)round(targetRect.mExtent[0] / 2.f)) / dimension[0],
        (float)(dimension[1] - targetRect.mCenter[1] + (int)round(targetRect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
    vertex[3].tcoord = {
        (float)(tcoordRect.mCenter[0] + (int)round(tcoordRect.mExtent[0] / 2.f)) / tcoordRect.mExtent[0],
        (float)(tcoordRect.mCenter[1] - (tcoordRect.mExtent[1] / 2)) / tcoordRect.mExtent[1] };
    vertex[3].color = SColorF(useColor[2]).ToArray();

    // Create the geometric object for drawing.
    Renderer::Get()->Update(visual->GetVertexBuffer());
    Renderer::Get()->Draw(visual);
}


/* 
    Replacement for Draw2DTexture that uses the high-quality pre-scaled texture, if configured.
 */
void UISkin::Draw2DTextureFilterScaled(
    const std::shared_ptr<Visual>& visual, const RectangleShape<2, int>& posRect, 
    const RectangleShape<2, int>& tcoordRect, const SColor* colors, const RectangleShape<2, int>* clip)
{
    auto effect = std::dynamic_pointer_cast<Texture2Effect>(visual->GetEffect());
    std::shared_ptr<Texture2> srcTexture = effect->GetTexture();

    // Attempt to pre-scale image in software in high quality.
    if (srcTexture)
    {
        std::shared_ptr<Texture2> scaled = AddScaledTexture(srcTexture, posRect, tcoordRect);
        if (!scaled) return;

        // Correct source rect based on scaled image.
        effect->SetTexture(scaled);
        const RectangleShape<2, int> textureRect = (scaled != srcTexture) ?
            RectangleShape<2, int>(Vector2<int>::Zero(), posRect.mAxis, posRect.mExtent) : tcoordRect;

        Draw2DTexture(visual, posRect, textureRect, colors, clip);
    }
}

void UISkin::Draw2DTexture9Slice(
    const std::shared_ptr<Visual>& visual, const RectangleShape<2, int>& frameRect, 
    const RectangleShape<2, int>& middle, const SColor* colors, const RectangleShape<2, int>* clip)
{
    auto effect = std::dynamic_pointer_cast<Texture2Effect>(visual->GetEffect());
    std::shared_ptr<Texture2> texture = effect->GetTexture();

    Vector2<int> textureSize = Vector2<int>{ 
        (int)texture->GetDimension(0), (int)texture->GetDimension(1) };
    Vector2<int> lowerRightCorner = Vector2<int>{
        middle.mCenter[0] + (int)round(middle.mExtent[0] / 2.f),
        middle.mCenter[1] + (int)round(middle.mExtent[1] / 2.f)};
    Vector2<int> lowerRightOffset = textureSize - lowerRightCorner;

    for (int y = 0; y < 3; ++y) 
    {
        for (int x = 0; x < 3; ++x) 
        {
            RectangleShape<2, int> tcoordRect(Vector2<int>::Zero(), frameRect.mAxis, textureSize);
            RectangleShape<2, int> posRect = frameRect;

            int leftCorner, rightCorner;
            switch (x) 
            {
                case 0:
                    posRect.mCenter[0] -= posRect.mExtent[0] / 2;
                    posRect.mExtent[0] = (frameRect.mCenter[0] - frameRect.mExtent[0] / 2) +
                        (middle.mCenter[0] - middle.mExtent[0] / 2) - posRect.mCenter[0];
                    posRect.mCenter[0] += posRect.mExtent[0] / 2;

                    tcoordRect.mCenter[0] -= tcoordRect.mExtent[0] / 2;
                    tcoordRect.mExtent[0] =
                        (middle.mCenter[0] - middle.mExtent[0] / 2) - tcoordRect.mCenter[0];
                    tcoordRect.mCenter[0] += tcoordRect.mExtent[0] / 2;
                    break;

                case 1:
                    leftCorner = posRect.mCenter[0] - posRect.mExtent[0] / 2;
                    leftCorner += (middle.mCenter[0] - middle.mExtent[0] / 2);
                    rightCorner = posRect.mCenter[0] + (int)round(posRect.mExtent[0] / 2.f);
                    rightCorner -= lowerRightOffset[0];
                    posRect.mCenter[0] = leftCorner;
                    posRect.mExtent[0] = rightCorner - leftCorner;
                    posRect.mCenter[0] += posRect.mExtent[0] / 2;

                    tcoordRect.mCenter[0] = middle.mCenter[0];
                    tcoordRect.mExtent[0] = middle.mExtent[0];
                    break;

                case 2:
                    posRect.mCenter[0] += (int)round(posRect.mExtent[0] / 2.f);
                    posRect.mExtent[0] = posRect.mCenter[0] -
                        ((frameRect.mCenter[0] + (int)round(frameRect.mExtent[0] / 2.f)) - lowerRightCorner[0]);
                    posRect.mCenter[0] -= (int)round(posRect.mExtent[0] / 2.f);

                    tcoordRect.mCenter[0] += (int)round(tcoordRect.mExtent[0] / 2.f);
                    tcoordRect.mExtent[0] = tcoordRect.mCenter[0] -
                        (middle.mCenter[0] + (int)round(middle.mExtent[0] / 2.f));
                    tcoordRect.mCenter[0] -= (int)round(tcoordRect.mExtent[0] / 2.f);
                    break;
            }

            switch (y) 
            {
                case 0:
                    posRect.mCenter[1] -= posRect.mExtent[1] / 2;
                    posRect.mExtent[1] =
                        (frameRect.mCenter[1] - frameRect.mExtent[1] / 2) +
                        (middle.mCenter[1] - middle.mExtent[1] / 2) - posRect.mCenter[1];
                    posRect.mCenter[1] += posRect.mExtent[1] / 2;

                    tcoordRect.mCenter[1] -= tcoordRect.mExtent[1] / 2;
                    tcoordRect.mExtent[1] =
                        (middle.mCenter[1] - middle.mExtent[1] / 2) - tcoordRect.mCenter[1];
                    tcoordRect.mCenter[1] += tcoordRect.mExtent[1] / 2;
                    break;

                case 1:
                    leftCorner = posRect.mCenter[1] - posRect.mExtent[1] / 2;
                    leftCorner += (middle.mCenter[1] - middle.mExtent[1] / 2);
                    rightCorner = posRect.mCenter[1] + (int)round(posRect.mExtent[1] / 2.f);
                    rightCorner -= lowerRightOffset[1];
                    posRect.mCenter[1] = leftCorner;
                    posRect.mExtent[1] = rightCorner - leftCorner;
                    posRect.mCenter[1] += posRect.mExtent[1] / 2;

                    tcoordRect.mCenter[1] = middle.mCenter[1];
                    tcoordRect.mExtent[1] = middle.mExtent[1];
                    break;

                case 2:
                    posRect.mCenter[1] += (int)round(posRect.mExtent[1] / 2.f);
                    posRect.mExtent[1] = posRect.mCenter[1] -
                        ((frameRect.mCenter[1] + (int)round(frameRect.mExtent[1] / 2.f)) - lowerRightCorner[1]);
                    posRect.mCenter[1] -= (int)round(posRect.mExtent[1] / 2.f);

                    tcoordRect.mCenter[1] += (int)round(tcoordRect.mExtent[1] / 2.f);
                    tcoordRect.mExtent[1] = tcoordRect.mCenter[1] -
                        (middle.mCenter[1] + (int)round(middle.mExtent[1] / 2.f));
                    tcoordRect.mCenter[1] -= (int)round(tcoordRect.mExtent[1] / 2.f);
                    break;
            }

            Draw2DTextureFilterScaled(visual, posRect, tcoordRect, colors, clip);
        }
    }
}

void UISkin::Draw2DLine(const SColorF &color, const Vector2<float>& start, const Vector2<float>& end)
{
    struct Vertex
    {
        Vector3<float> position;
        Vector4<float> color;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 2);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);
    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_POLYSEGMENT_DISJOINT, 1);

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
    std::shared_ptr<Visual> visual = std::make_shared<Visual>(vbuffer, ibuffer, effect);

    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
    Vector2<int> dimension = { (int)screenSize[0] / 2, (int)screenSize[1] / 2 };

    std::array<float, 4U> cl = color.ToArray();

    Vertex* vertex = vbuffer->Get<Vertex>();
    vertex[0].position = Vector3<float>{
        (float)(start[0] - dimension[0]) / dimension[0], 
        (float)(dimension[1] - start[1]) / dimension[1], 0.f };
    vertex[0].color = cl;
    vertex[1].position = Vector3<float>{
        (float)(end[0] - dimension[0]) / dimension[0], 
        (float)(dimension[1] - end[1]) / dimension[1], 0.f };
    vertex[1].color = cl;

    Renderer::Get()->Update(vbuffer);
    Renderer::Get()->Draw(visual);
}

//! draws a 2d rectangle.
void UISkin::Draw2DRectangle(const SColorF &color, const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip)
{
	if (!Renderer::Get())
		return;

	RectangleShape<2, int> rect = frameRect;
    if (clip)
        rect.ClipAgainst(*clip);

    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
    Vector2<int> dimension = { (int)screenSize[0] / 2, (int)screenSize[1] / 2};

	struct Vertex
	{
		Vector3<float> position;
		Vector4<float> color;
	};
	Vertex* vertex = visual->GetVertexBuffer()->Get<Vertex>();

    std::array<float, 4U> cl = color.ToArray();
	vertex[0].position = {
		(float)(rect.mCenter[0] - dimension[0] - (rect.mExtent[0] / 2)) / dimension[0],
		(float)(dimension[1] - rect.mCenter[1] - (rect.mExtent[1] / 2)) / dimension[1], 0.0f };
	vertex[0].color = cl;
	vertex[1].position = {
		(float)(rect.mCenter[0] - dimension[0] + (int)round(rect.mExtent[0] / 2.f)) / dimension[0],
		(float)(dimension[1] - rect.mCenter[1] - (rect.mExtent[1] / 2)) / dimension[1], 0.0f };
	vertex[1].color = cl;
	vertex[2].position = {
		(float)(rect.mCenter[0] - dimension[0] - (rect.mExtent[0] / 2)) / dimension[0],
		(float)(dimension[1] - rect.mCenter[1] + (int)round(rect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
	vertex[2].color = cl;
	vertex[3].position = {
		(float)(rect.mCenter[0] - dimension[0] + (int)round(rect.mExtent[0] / 2.f)) / dimension[0],
		(float)(dimension[1] - rect.mCenter[1] + (int)round(rect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
	vertex[3].color = cl;

	// Create the geometric object for drawing.
	Renderer::Get()->Update(visual->GetVertexBuffer());
	Renderer::Get()->Draw(visual);
}


//! draws a 2d rectangle.
void UISkin::Draw2DRectangle(
    const SColorF &color1, const SColorF &color2, const std::shared_ptr<Visual>& visual, 
    const RectangleShape<2, int>& frameRect, const RectangleShape<2, int>* clip)
{
    if (!Renderer::Get())
        return;

    RectangleShape<2, int> rect = frameRect;
    if (clip)
        rect.ClipAgainst(*clip);

    Vector2<unsigned int> screenSize = Renderer::Get()->GetScreenSize();
    Vector2<int> dimension = { (int)screenSize[0] / 2, (int)screenSize[1] / 2 };

    struct Vertex
    {
        Vector3<float> position;
        Vector4<float> color;
    };
    Vertex* vertex = visual->GetVertexBuffer()->Get<Vertex>();

    std::array<float, 4U> c1 = color1.ToArray();
    std::array<float, 4U> c2 = color2.ToArray();

    vertex[0].position = {
        (float)(rect.mCenter[0] - dimension[0] - (rect.mExtent[0] / 2)) / dimension[0],
        (float)(dimension[1] - rect.mCenter[1] - (rect.mExtent[1] / 2)) / dimension[1], 0.0f };
    vertex[0].color = { c2[0], c2[1], c2[2], c2[3] };
    vertex[1].position = {
        (float)(rect.mCenter[0] - dimension[0] + (int)round(rect.mExtent[0] / 2.f)) / dimension[0],
        (float)(dimension[1] - rect.mCenter[1] - (rect.mExtent[1] / 2)) / dimension[1], 0.0f };
    vertex[1].color = { c2[0], c2[1], c2[2], c2[3] };
    vertex[2].position = {
        (float)(rect.mCenter[0] - dimension[0] - (rect.mExtent[0] / 2)) / dimension[0],
        (float)(dimension[1] - rect.mCenter[1] + (int)round(rect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
    vertex[2].color = { c1[0], c1[1], c1[2], c1[3] };
    vertex[3].position = {
        (float)(rect.mCenter[0] - dimension[0] + (int)round(rect.mExtent[0] / 2.f)) / dimension[0],
        (float)(dimension[1] - rect.mCenter[1] + (int)round(rect.mExtent[1] / 2.f)) / dimension[1], 0.0f };
    vertex[3].color = { c1[0], c1[1], c1[2], c1[3] };

    // Create the geometric object for drawing.
    Renderer::Get()->Update(visual->GetVertexBuffer());
    Renderer::Get()->Draw(visual);
}


UISkinThemeType UISkin::GetType() const
{
	return mType;
}