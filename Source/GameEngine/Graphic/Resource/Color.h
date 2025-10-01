// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef COLOR_H
#define COLOR_H

#include "Core/CoreStd.h"

#include "Graphic/GraphicStd.h"

#include "DataFormat.h"


//! Returns the alpha component from A1R5G5B5 color
/** Alpha refers to opacity.
\return The alpha value of the color. 0 is transparent, 1 is opaque. */
inline uint32_t GetAlpha(uint16_t color)
{
	return ((color >> 15)&0x1);
}


//! Returns the red component from A1R5G5B5 color.
/** Shift left by 3 to get 8 bit value. */
inline uint32_t GetRed(uint16_t color)
{
	return ((color >> 10)&0x1F);
}


//! Returns the green component from A1R5G5B5 color
/** Shift left by 3 to get 8 bit value. */
inline uint32_t GetGreen(uint16_t color)
{
	return ((color >> 5)&0x1F);
}


//! Returns the blue component from A1R5G5B5 color
/** Shift left by 3 to get 8 bit value. */
inline uint32_t GetBlue(uint16_t color)
{
	return (color & 0x1F);
}


//! Returns the average from a 16 bit A1R5G5B5 color
inline int32_t GetAverage(int16_t color)
{
	return ((GetRed(color)<<3) + (GetGreen(color)<<3) + (GetBlue(color)<<3)) / 3;
}

//! Class representing a 32 bit ARGB color.
/** The color values for alpha, red, green, and blue are
stored in a single u32. So all four values may be between 0 and 255.
Alpha in engine is opacity, so 0 is fully transparent, 255 is fully opaque (solid).
This class is used by most parts of the Engine
to specify a color. Another way is using the class SColorf, which
stores the color values in 4 floats.
This class must consist of only one u32 and must not use virtual functions.
*/
class SColor
{
public:

	//! Constructor of the Color. Does nothing.
	/** The color value is not initialized to save time. */
	SColor() {}

	//! Constructs the color from 4 values representing the alpha, red, green and blue component.
	/** Must be values between 0 and 255. */
	SColor (uint32_t a, uint32_t r, uint32_t g, uint32_t b)
		: mColor(((a & 0xff)<<24) | ((r & 0xff)<<16) | ((g & 0xff)<<8) | (b & 0xff)) {}

	//! Constructs the color from a 32 bit value. Could be another color.
	SColor(uint32_t clr)
		: mColor(clr) {}

	//! Returns the alpha component of the color.
	/** The alpha component defines how opaque a color is.
	\return The alpha value of the color. 0 is fully transparent, 255 is fully opaque. */
    uint32_t GetAlpha() const { return mColor >>24; }

	//! Returns the red component of the color.
	/** \return Value between 0 and 255, specifying how red the color is.
	0 means no red, 255 means full red. */
    uint32_t GetRed() const { return (mColor >>16) & 0xff; }

	//! Returns the green component of the color.
	/** \return Value between 0 and 255, specifying how green the color is.
	0 means no green, 255 means full green. */
    uint32_t GetGreen() const { return (mColor >>8) & 0xff; }

	//! Returns the blue component of the color.
	/** \return Value between 0 and 255, specifying how blue the color is.
	0 means no blue, 255 means full blue. */
    uint32_t GetBlue() const { return mColor & 0xff; }

	//! Get lightness of the color in the range [0,255]
	float GetLightness() const
	{
		return 0.5f*(
            std::max(std::max(GetRed(),GetGreen()),GetBlue())+
            std::min(std::min(GetRed(),GetGreen()),GetBlue()));
	}

	//! Get luminance of the color in the range [0,255].
	float GetLuminance() const
	{
		return 0.3f*GetRed() + 0.59f*GetGreen() + 0.11f*GetBlue();
	}

	//! Get average intensity of the color in the range [0,255].
    uint32_t GetAverage() const
	{
		return ( GetRed() + GetGreen() + GetBlue() ) / 3;
	}

	//! Sets the alpha component of the Color.
	/** The alpha component defines how transparent a color should be.
	\param a The alpha value of the color. 0 is fully transparent, 255 is fully opaque. */
	void SetAlpha(uint32_t a) { mColor = ((a & 0xff)<<24) | (mColor & 0x00ffffff); }

	//! Sets the red component of the Color.
	/** \param r: Has to be a value between 0 and 255.
	0 means no red, 255 means full red. */
	void SetRed(uint32_t r) { mColor = ((r & 0xff)<<16) | (mColor & 0xff00ffff); }

	//! Sets the green component of the Color.
	/** \param g: Has to be a value between 0 and 255.
	0 means no green, 255 means full green. */
	void SetGreen(uint32_t g) { mColor = ((g & 0xff)<<8) | (mColor & 0xffff00ff); }

	//! Sets the blue component of the Color.
	/** \param b: Has to be a value between 0 and 255.
	0 means no blue, 255 means full blue. */
	void SetBlue(uint32_t b) { mColor = (b & 0xff) | (mColor & 0xffffff00); }


	//! Sets all four components of the color at once.
	/** Constructs the color from 4 values representing the alpha,
	red, green and blue components of the color. Must be values
	between 0 and 255.
	\param a: Alpha component of the color. The alpha component
	defines how transparent a color should be. Has to be a value
	between 0 and 255. 255 means not transparent (opaque), 0 means
	fully transparent.
	\param r: Sets the red component of the Color. Has to be a
	value between 0 and 255. 0 means no red, 255 means full red.
	\param g: Sets the green component of the Color. Has to be a
	value between 0 and 255. 0 means no green, 255 means full
	green.
	\param b: Sets the blue component of the Color. Has to be a
	value between 0 and 255. 0 means no blue, 255 means full blue. */
	void Set(uint32_t a, uint32_t r, uint32_t g, uint32_t b)
	{
        mColor = (((a & 0xff)<<24) | ((r & 0xff)<<16) | ((g & 0xff)<<8) | (b & 0xff));
	}
	void Set(uint32_t col) { mColor = col; }

	//! Compares the color to another color.
	/** \return True if the colors are the same, and false if not. */
	bool operator==(const SColor& other) const { return other.mColor == mColor; }

	//! Compares the color to another color.
	/** \return True if the colors are different, and false if they are the same. */
	bool operator!=(const SColor& other) const { return other.mColor != mColor; }

	//! comparison operator
	/** \return True if this color is smaller than the other one */
	bool operator<(const SColor& other) const { return (mColor < other.mColor); }

	//! Adds two colors, result is clamped to 0..255 values
	/** \param other Color to add to this color
	\return Addition of the two colors, clamped to 0..255 values */
	SColor operator+(const SColor& other) const
	{
		return SColor(std::min(GetAlpha() + other.GetAlpha(), 255u),
                        std::min(GetRed() + other.GetRed(), 255u),
                        std::min(GetGreen() + other.GetGreen(), 255u),
                        std::min(GetBlue() + other.GetBlue(), 255u));
	}

	//! Interpolates the color with a float value to another color
	/** \param other: Other color
	\param d: value between 0.0f and 1.0f. d=0 returns other, d=1 returns this, values between interpolate.
	\return Interpolated color. */
    SColor GetInterpolated(const SColor &other, float d) const;

	//! Returns interpolated color. ( quadratic )
	/** \param c1: first color to interpolate with
	\param c2: second color to interpolate with
	\param d: value between 0.0f and 1.0f. */
    SColor GetInterpolatedQuadratic(const SColor& c1, const SColor& c2, float d) const;

	//! set the color by expecting data in the given format
	/** \param data: must point to valid memory containing color information in the given format
		\param format: tells the format in which data is available
	*/
	void SetData(void* data, DFType type = DF_R8G8B8A8_UNORM)
	{
		switch (type)
		{
			case DFType::DF_R8G8B8A8_UNORM:
			{
				uint8_t* pixel = reinterpret_cast<uint8_t*>(data);
				Set(pixel[3], pixel[0], pixel[1], pixel[2]);
			}
			break;
		}
	}

	//! Write the color to data in the defined format
	/** \param data: target to write the color. Must contain sufficiently large memory to receive the number of bytes neede for format
		\param format: tells the format used to write the color into data
	*/
	void GetData(void* dest, DFType type = DF_R8G8B8A8_UNORM) const
	{
		switch (type)
		{
			case DFType::DF_R8G8B8A8_UNORM:
			{
				uint8_t* pixel = reinterpret_cast<uint8_t*>(dest);
				pixel[0] = (uint8_t)GetRed();
				pixel[1] = (uint8_t)GetGreen();
				pixel[2] = (uint8_t)GetBlue();
				pixel[3] = (uint8_t)GetAlpha();
			}
			break;
		}
	}

	//! color in A8R8G8B8 Format
    uint32_t mColor;
};


//! Class representing a color with four floats.
/** The color values for red, green, blue
and alpha are each stored in a 32 bit floating point variable.
So all four values may be between 0.0f and 1.0f.
Another, faster way to define colors is using the class SColor, which
stores the color values in a single 32 bit integer.
*/
class SColorF
{
public:
	//! Default constructor for SColorf.
	/** Sets red, green and blue to 0.0f and alpha to 1.0f. */
	SColorF() : mRed(0.0f), mGreen(0.0f), mBlue(0.0f), mAlpha(1.0f) {}

	//! Constructs a color from up to four color values: red, green, blue, and alpha.
	/** \param r: Red color component. Should be a value between
	0.0f meaning no red and 1.0f, meaning full red.
	\param g: Green color component. Should be a value between 0.0f
	meaning no green and 1.0f, meaning full green.
	\param b: Blue color component. Should be a value between 0.0f
	meaning no blue and 1.0f, meaning full blue.
	\param a: Alpha color component of the color. The alpha
	component defines how transparent a color should be. Has to be
	a value between 0.0f and 1.0f, 1.0f means not transparent
	(opaque), 0.0f means fully transparent. */
    SColorF(float r, float g, float b, float a = 1.0f) : mRed(r), mGreen(g), mBlue(b), mAlpha(a) {}

	//! Constructs a color from 32 bit Color.
	/** \param c: 32 bit color from which this SColorf class is
	constructed from. */
    SColorF(SColor c)
	{
		const float inv = 1.0f / 255.0f;
        mRed = c.GetRed() * inv;
        mGreen = c.GetGreen() * inv;
        mBlue = c.GetBlue() * inv;
        mAlpha = c.GetAlpha() * inv;
	}

    SColorF(std::array<float, 4U> c)
    {
        mRed = c[0];
        mGreen = c[1];
        mBlue = c[2];
        mAlpha = c[3];
    }

	//! Converts this color to a SColor without floats.
	SColor ToSColor() const
	{
		return SColor(
            (uint32_t)round(mAlpha*255.0f), (uint32_t)round(mRed*255.0f),
            (uint32_t)round(mGreen*255.0f), (uint32_t)round(mBlue*255.0f));
	}

    std::array<float, 4U> ToArray() const
    {
        return {mRed, mGreen, mBlue, mAlpha};
    }

	//! Sets three color components to new values at once.
	/** \param rr: Red color component. Should be a value between 0.0f meaning
	no red (=black) and 1.0f, meaning full red.
	\param gg: Green color component. Should be a value between 0.0f meaning
	no green (=black) and 1.0f, meaning full green.
	\param bb: Blue color component. Should be a value between 0.0f meaning
	no blue (=black) and 1.0f, meaning full blue. */
	void Set(float rr, float gg, float bb) { mRed = rr; mGreen =gg; mBlue = bb; }

	//! Sets all four color components to new values at once.
	/** \param aa: Alpha component. Should be a value between 0.0f meaning
	fully transparent and 1.0f, meaning opaque.
	\param rr: Red color component. Should be a value between 0.0f meaning
	no red and 1.0f, meaning full red.
	\param gg: Green color component. Should be a value between 0.0f meaning
	no green and 1.0f, meaning full green.
	\param bb: Blue color component. Should be a value between 0.0f meaning
	no blue and 1.0f, meaning full blue. */
	void Set(float aa, float rr, float gg, float bb) { mAlpha = aa; mRed = rr; mGreen =gg; mBlue = bb; }

    //! Compares the color to another color.
    /** \return True if the colors are the same, and false if not. */
    bool operator==(const SColorF& other) const 
    { 
        return other.mAlpha == mAlpha && 
                other.mRed == mRed &&
                other.mBlue == mBlue &&
                other.mGreen == mGreen;
    }


    //! Compares the color to another color.
    /** \return True if the colors are different, and false if they are the same. */
    bool operator!=(const SColorF& other) const
    {
        return other.mAlpha != mAlpha ||
            other.mRed != mRed ||
            other.mBlue != mBlue ||
            other.mGreen != mGreen;
    }

	//! Interpolates the color with a float value to another color
	/** \param other: Other color
	\param d: value between 0.0f and 1.0f
	\return Interpolated color. */
    SColorF GetInterpolated(const SColorF &other, float d) const;

	//! Returns interpolated color. ( quadratic )
	/** \param c1: first color to interpolate with
	\param c2: second color to interpolate with
	\param d: value between 0.0f and 1.0f. */
    SColorF GetInterpolatedQuadratic(const SColorF& c1, const SColorF& c2, float d) const;

	//! Sets a color component by index. R=0, G=1, B=2, A=3
    void SetColorComponentValue(int32_t index, float value);

	//! Returns the alpha component of the color in the range 0.0 (transparent) to 1.0 (opaque)
	float GetAlpha() const { return mAlpha; }

	//! Returns the red component of the color in the range 0.0 to 1.0
    float GetRed() const { return mRed; }

	//! Returns the green component of the color in the range 0.0 to 1.0
    float GetGreen() const { return mGreen; }

	//! Returns the blue component of the color in the range 0.0 to 1.0
    float GetBlue() const { return mBlue; }

	//! red color component
    float mRed;

	//! green color component
	float mGreen;

	//! blue component
    float mBlue;

	//! alpha color component
    float mAlpha;
};


//! Class representing a color in HSL format
/** The color values for hue, saturation, luminance
are stored in 32bit floating point variables. Hue is in range [0,360],
Luminance and Saturation are in percent [0,100]
*/
class SColorHSL
{
public:
	SColorHSL ( float h = 0.f, float s = 0.f, float l = 0.f )
		: mHue ( h ), mSaturation ( s ), mLuminance ( l ) {}

	void FromRGB(const SColorF &color);
	void ToRGB(SColorF &color) const;

    float mHue;
    float mSaturation;
    float mLuminance;

private:
	inline float ToRGB1(float rm1, float rm2, float rh) const;

};



#endif
