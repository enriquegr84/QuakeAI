// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/11/28)

#include "Core/Logger/Logger.h"

#include "Color.h"

#include "Mathematic/Function/Functions.h"

SColor SColor::GetInterpolated(const SColor &other, float d) const
{
    d = std::clamp(d, 0.f, 1.f);
    const float inv = 1.0f - d;
    return SColor((uint32_t)round(other.GetAlpha()*inv + GetAlpha()*d),
        (uint32_t)round(other.GetRed()*inv + GetRed()*d),
        (uint32_t)round(other.GetGreen()*inv + GetGreen()*d),
        (uint32_t)round(other.GetBlue()*inv + GetBlue()*d));
}


SColor SColor::GetInterpolatedQuadratic(const SColor& c1, const SColor& c2, float d) const
{
    // this*(1-d)*(1-d) + 2 * c1 * (1-d) + c2 * d * d;
    d = std::clamp(d, 0.f, 1.f);
    const float inv = 1.f - d;
    const float mul0 = inv * inv;
    const float mul1 = 2.f * d * inv;
    const float mul2 = d * d;

    return SColor(
        std::clamp((int)floor(
            GetAlpha() * mul0 + c1.GetAlpha() * mul1 + c2.GetAlpha() * mul2), 0, 255),
        std::clamp((int)floor(
            GetRed()   * mul0 + c1.GetRed()   * mul1 + c2.GetRed()   * mul2), 0, 255),
        std::clamp((int)floor(
            GetGreen() * mul0 + c1.GetGreen() * mul1 + c2.GetGreen() * mul2), 0, 255),
        std::clamp((int)floor(
            GetBlue()  * mul0 + c1.GetBlue()  * mul1 + c2.GetBlue()  * mul2), 0, 255));
}


SColorF SColorF::GetInterpolated(const SColorF &other, float d) const
{
    d = std::clamp(d, 0.f, 1.f);
    const float inv = 1.0f - d;
    return SColorF(
        other.mRed*inv + mRed * d, other.mGreen*inv + mGreen * d,
        other.mBlue*inv + mBlue * d, other.mAlpha*inv + mAlpha * d);
}


SColorF SColorF::GetInterpolatedQuadratic(const SColorF& c1, const SColorF& c2, float d) const
{
    d = std::clamp(d, 0.f, 1.f);
    // this*(1-d)*(1-d) + 2 * c1 * (1-d) + c2 * d * d;
    const float inv = 1.f - d;
    const float mul0 = inv * inv;
    const float mul1 = 2.f * d * inv;
    const float mul2 = d * d;

    return SColorF(
        mRed * mul0 + c1.mRed * mul1 + c2.mRed * mul2,
        mGreen * mul0 + c1.mGreen * mul1 + c2.mGreen * mul2,
        mBlue * mul0 + c1.mBlue * mul1 + c2.mBlue * mul2,
        mAlpha * mul0 + c1.mAlpha * mul1 + c2.mAlpha * mul2);
}


void SColorF::SetColorComponentValue(int32_t index, float value)
{
    switch (index)
    {
        case 0: mRed = value; break;
        case 1: mGreen = value; break;
        case 2: mBlue = value; break;
        case 3: mAlpha = value; break;
    }
}


void SColorHSL::FromRGB(const SColorF &color)
{
    const float maxVal = Function<float>::Max(color.GetRed(), color.GetGreen(), color.GetBlue());
    const float minVal = Function<float>::Min(color.GetRed(), color.GetGreen(), color.GetBlue());
    mLuminance = (maxVal + minVal) * 50;
    if (Function<float>::Equals(maxVal, minVal))
    {
        mHue = 0.f;
        mSaturation = 0.f;
        return;
    }

    const float delta = maxVal - minVal;
    if (mLuminance <= 50)
    {
        mSaturation = (delta) / (maxVal + minVal);
    }
    else
    {
        mSaturation = (delta) / (2 - maxVal - minVal);
    }
    mSaturation *= 100;

    if (Function<float>::Equals(maxVal, color.GetRed()))
        mHue = (color.GetGreen() - color.GetBlue()) / delta;
    else if (Function<float>::Equals(maxVal, color.GetGreen()))
        mHue = 2 + ((color.GetBlue() - color.GetRed()) / delta);
    else // blue is max
        mHue = 4 + ((color.GetRed() - color.GetGreen()) / delta);

    mHue *= 60.0f;
    while (mHue < 0.f)
        mHue += 360;
}


void SColorHSL::ToRGB(SColorF &color) const
{
    const float l = mLuminance / 100;
    if (Function<float>::IsZero(mSaturation)) // grey
    {
        color.Set(l, l, l);
        return;
    }

    float rm2;

    if (mLuminance <= 50)
    {
        rm2 = l + l * (mSaturation / 100);
    }
    else
    {
        rm2 = l + (1 - l) * (mSaturation / 100);
    }

    const float rm1 = 2.0f * l - rm2;

    const float h = mHue / 360.0f;
    color.Set(
        ToRGB1(rm1, rm2, h + 1.f / 3.f),
        ToRGB1(rm1, rm2, h),
        ToRGB1(rm1, rm2, h - 1.f / 3.f));
}


float SColorHSL::ToRGB1(float rm1, float rm2, float rh) const
{
    if (rh < 0)
        rh += 1;
    if (rh > 1)
        rh -= 1;

    if (rh < 1.f / 6.f)
        rm1 = rm1 + (rm2 - rm1) * rh*6.f;
    else if (rh < 0.5f)
        rm1 = rm2;
    else if (rh < 2.f / 3.f)
        rm1 = rm1 + (rm2 - rm1) * ((2.f / 3.f) - rh)*6.f;

    return rm1;
}

