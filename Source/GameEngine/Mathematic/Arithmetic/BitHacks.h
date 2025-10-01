// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef BITHACKS_H
#define BITHACKS_H

#include "Mathematic/MathematicStd.h"

#include <memory>
#include <cstdint>

// Convenient macros for manipulating 64-bit integers.
#define GE_I64(v) v##LL
#define GE_U64(v) v##ULL
#define GE_GET_LO_I64(v) (int32_t)((v) & 0x00000000ffffffffLL)
#define GE_GET_HI_I64(v) (int32_t)(((v) >> 32) & 0x00000000ffffffffLL)
#define GE_GET_LO_U64(v) (uint32_t)((v) & 0x00000000ffffffffULL)
#define GE_GET_HI_U64(v) (uint32_t)(((v) >> 32) & 0x00000000ffffffffULL)
#define GE_SET_LO_I64(v,lo) (((v) & 0xffffffff00000000LL) | (int64_t)(lo))
#define GE_SET_HI_I64(v,hi) (((v) & 0x00000000ffffffffLL) | ((int64_t)(hi) << 32))
#define GE_MAKE_I64(hi,lo)  ((int64_t)(lo) | ((int64_t)(hi) << 32))
#define GE_SET_LO_U64(v,lo) (((v) & 0xffffffff00000000ULL) | (uint64_t)(lo))
#define GE_SET_HI_U64(v,hi) (((v) & 0x00000000ffffffffULL) | ((uint64_t)(hi) << 32))
#define GE_MAKE_U64(hi,lo)  ((uint64_t)(lo) | ((uint64_t)(hi) << 32))


// Compute next-higher power of 2 efficiently, e.g. for power-of-2 texture sizes.
// Public Domain: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
MATH_ITEM uint32_t NextPowerOfTwo(uint32_t orig);

MATH_ITEM bool IsPowerOfTwo(uint32_t value);
MATH_ITEM bool IsPowerOfTwo(int32_t value);

MATH_ITEM uint32_t Log2OfPowerOfTwo(uint32_t powerOfTwo);
MATH_ITEM int32_t Log2OfPowerOfTwo(int32_t powerOfTwo);

// Call these only for nonzero values.  If value is zero, then GetLeadingBit
// and GetTrailingBit return zero.
MATH_ITEM int32_t GetLeadingBit(uint32_t value);
MATH_ITEM int32_t GetLeadingBit(int32_t value);
MATH_ITEM int32_t GetLeadingBit(uint64_t value);
MATH_ITEM int32_t GetLeadingBit(int64_t value);
MATH_ITEM int32_t GetTrailingBit(uint32_t value);
MATH_ITEM int32_t GetTrailingBit(int32_t value);
MATH_ITEM int32_t GetTrailingBit(uint64_t value);
MATH_ITEM int32_t GetTrailingBit(int64_t value);

MATH_ITEM uint32_t GetBits(uint32_t x, uint32_t pos, uint32_t len);

MATH_ITEM void SetBits(uint32_t* x, uint32_t pos, uint32_t len, uint32_t val);

MATH_ITEM uint32_t CalculateParity(uint32_t v);

// Round up to a power of two.  If input is zero, the return is 1.  If input
// is larger than 2^{31}, the return is 2^{32}.
MATH_ITEM uint64_t RoundUpToPowerOfTwo(uint32_t value);

// Round down to a power of two.  If input is zero, the return is 0.
MATH_ITEM uint32_t RoundDownToPowerOfTwo(uint32_t value);

// 64-bit unaligned version of MurmurHash
MATH_ITEM uint64_t MurmurHash64ua(const void* key, int len, unsigned int seed);

#endif