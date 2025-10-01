/*
 * Conversion of float to IEEE-754 and vice versa.
 *
 * © Copyright 2018 Pedro Gimeno Fortea.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "IEEEFloat.h"

#include "Core/Logger/Logger.h"

#include <limits>
#include <cmath>

// Given an unsigned 32-bit integer representing an IEEE-754 single-precision
// float, return the float.
float UInt32ToFloatSlow(uint32_t i)
{
	// clang-format off
	int exp = (i >> 23) & 0xFF;
	uint32_t sign = i & 0x80000000UL;
	uint32_t imant = i & 0x7FFFFFUL;
	if (exp == 0xFF) 
    {
		// Inf/NaN
		if (imant == 0) 
        {
			if (std::numeric_limits<float>::has_infinity)	
				return sign ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
			return sign ? std::numeric_limits<float>::max() : std::numeric_limits<float>::lowest();
		}
		return std::numeric_limits<float>::has_quiet_NaN ? std::numeric_limits<float>::quiet_NaN() : -0.f;
	}

	if (!exp) {
		// Denormal or zero
		return sign ? -ldexpf((float)imant, -149) : ldexpf((float)imant, -149);
	}

	return sign ? -ldexpf((float)(imant | 0x800000UL), exp - 150) :
		ldexpf((float)(imant | 0x800000UL), exp - 150);
	// clang-format on
}

// Given a float, return an unsigned 32-bit integer representing the float
// in IEEE-754 single-precision format.
uint32_t FloatToUInt32Slow(float f)
{
	uint32_t signbit = std::copysign(1.0f, f) == 1.0f ? 0 : 0x80000000UL;
	if (f == 0.f)
		return signbit;
	if (std::isnan(f))
		return signbit | 0x7FC00000UL;
	if (std::isinf(f))
		return signbit | 0x7F800000UL;
	int exp = 0; // silence warning
	float mant = frexpf(f, &exp);
	uint32_t imant = (uint32_t)std::floor((signbit ? -16777216.f : 16777216.f) * mant);
	exp += 126;
	if (exp <= 0) {
		// Denormal
		return signbit | (exp <= -31 ? 0 : imant >> (1 - exp));
	}

	if (exp >= 255) {
		// Overflow due to the platform having exponents bigger than IEEE ones.
		// Return signed infinity.
		return signbit | 0x7F800000UL;
	}

	// Regular number
	return signbit | (exp << 23) | (imant & 0x7FFFFFUL);
}

// This test needs the following requisites in order to work:
// - The float type must be a 32 bits IEEE-754 single-precision float.
// - The endianness of floats and integers must match.
FloatType GetFloatSerializationType()
{
	// clang-format off
	const float cf = -22220490.f;
	const uint32_t cu = 0xCBA98765UL;
	if (std::numeric_limits<float>::is_iec559 && sizeof(cf) == 4 &&
			sizeof(cu) == 4 && !memcmp(&cf, &cu, 4)) 
    {
		// UInt32ToFloatSlow and FloatToUInt32Slow are not needed, use memcpy
		return FLOATTYPE_SYSTEM;
	}

	// Run quick tests to ensure the custom functions provide acceptable results
	LogWarning("floatSerialization: float and uint32_t endianness are "
		"not equal or machine is not IEEE-754 compliant");
	uint32_t i;
	char buf[128];

	// NaN checks aren't included in the main loop
	if (!std::isnan(UInt32ToFloatSlow(0x7FC00000UL)))
    {
		std::snprintf(buf, sizeof(buf),
			"UInt32ToFloatSlow(0x7FC00000) failed to produce a NaN, actual: %.9g",
			UInt32ToFloatSlow(0x7FC00000UL));
		LogInformation(buf);
	}
	if (!std::isnan(UInt32ToFloatSlow(0xFFC00000UL))) 
    {
        std::snprintf(buf, sizeof(buf),
			"UInt32ToFloatSlow(0xFFC00000) failed to produce a NaN, actual: %.9g",
			UInt32ToFloatSlow(0xFFC00000UL));
		LogInformation(buf);
	}

	i = FloatToUInt32Slow(std::numeric_limits<float>::quiet_NaN());
	// check that it corresponds to a NaN encoding
	if ((i & 0x7F800000UL) != 0x7F800000UL || (i & 0x7FFFFFUL) == 0) 
    {
		std::snprintf(buf, sizeof(buf),
			"FloatToUInt32Slow(NaN) failed to encode NaN, actual: 0x%X", i);
		LogInformation(buf);
	}

	return FLOATTYPE_SLOW;
	// clang-format on
}
