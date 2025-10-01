// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/02/06)

#include "BitHacks.h"

static int32_t const gsLeadingBitTable[32] = 
{
     0,  9,  1, 10, 13, 21,  2, 29,
    11, 14, 16, 18, 22, 25,  3, 30,
     8, 12, 20, 28, 15, 17, 24,  7,
    19, 27, 23,  6, 26,  5,  4, 31
};

static int32_t const gsTrailingBitTable[32] = 
{
     0,  1, 28,  2, 29, 14, 24,  3,
    30, 22, 20, 15, 25, 17,  4,  8, 
    31, 27, 13, 23, 21, 19, 16,  7,
    26, 12, 18,  6, 11,  5, 10,  9
};


uint32_t NextPowerOfTwo(uint32_t orig)
{
    orig--;
    orig |= orig >> 1;
    orig |= orig >> 2;
    orig |= orig >> 4;
    orig |= orig >> 8;
    orig |= orig >> 16;
    return orig + 1;
}

bool IsPowerOfTwo(uint32_t value)
{
    return (value > 0) && ((value & (value - 1)) == 0);
}

bool IsPowerOfTwo(int32_t value)
{
    return (value > 0) && ((value & (value - 1)) == 0);
}

uint32_t Log2OfPowerOfTwo(uint32_t powerOfTwo)
{
    uint32_t log2 = (powerOfTwo & 0xAAAAAAAAu) != 0;
    log2 |= ((powerOfTwo & 0xFFFF0000u) != 0) << 4;
    log2 |= ((powerOfTwo & 0xFF00FF00u) != 0) << 3;
    log2 |= ((powerOfTwo & 0xF0F0F0F0u) != 0) << 2;
    log2 |= ((powerOfTwo & 0xCCCCCCCCu) != 0) << 1;
    return log2;
}

int32_t Log2OfPowerOfTwo(int32_t powerOfTwo)
{
    uint32_t log2 = (powerOfTwo & 0xAAAAAAAAu) != 0;
    log2 |= ((powerOfTwo & 0xFFFF0000u) != 0) << 4;
    log2 |= ((powerOfTwo & 0xFF00FF00u) != 0) << 3;
    log2 |= ((powerOfTwo & 0xF0F0F0F0u) != 0) << 2;
    log2 |= ((powerOfTwo & 0xCCCCCCCCu) != 0) << 1;
    return static_cast<int32_t>(log2);
}

int32_t GetLeadingBit(uint32_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    uint32_t key = (value * 0x07C4ACDDu) >> 27;
    return gsLeadingBitTable[key];
}

int32_t GetLeadingBit(int32_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    uint32_t key = (value * 0x07C4ACDDu) >> 27;
    return gsLeadingBitTable[key];
}

int32_t GetLeadingBit(uint64_t value)
{
    uint32_t v1 = GE_GET_HI_U64(value);
    if (v1 != 0)
    {
        return GetLeadingBit(v1) + 32;
    }

    uint32_t v0 = GE_GET_LO_U64(value);
    return GetLeadingBit(v0);
}

int32_t GetLeadingBit(int64_t value)
{
    int32_t v1 = GE_GET_HI_I64(value);
    if (v1 != 0)
    {
        return GetLeadingBit(v1) + 32;
    }

    int32_t v0 = GE_GET_LO_I64(value);
    return GetLeadingBit(v0);
}

int32_t GetTrailingBit(uint32_t value)
{
    // Avoid warning for negation of unsigned 'value'.
    int32_t const& iValue = reinterpret_cast<int32_t const&>(value);
    uint32_t key = ((uint32_t)((iValue & -iValue) * 0x077CB531u)) >> 27;
    return gsTrailingBitTable[key];
}

int32_t GetTrailingBit(int32_t value)
{
    uint32_t key = ((uint32_t)((value & -value) * 0x077CB531u)) >> 27;
    return gsTrailingBitTable[key];
}

int32_t GetTrailingBit(uint64_t value)
{
    uint32_t v0 = GE_GET_LO_U64(value);
    if (v0 != 0)
    {
        return GetTrailingBit(v0);
    }

    uint32_t v1 = GE_GET_HI_U64(value);
    return GetTrailingBit(v1) + 32;
}

int32_t GetTrailingBit(int64_t value)
{
    int32_t v0 = GE_GET_LO_I64(value);
    if (v0 != 0)
    {
        return GetTrailingBit(v0);
    }

    int32_t v1 = GE_GET_HI_I64(value);
    return GetTrailingBit(v1) + 32;
}

uint32_t GetBits(uint32_t x, uint32_t pos, uint32_t len)
{
    uint32_t mask = (1 << len) - 1;
    return (x >> pos) & mask;
}

void SetBits(uint32_t* x, uint32_t pos, uint32_t len, uint32_t val)
{
    uint32_t mask = (1 << len) - 1;
    *x &= ~(mask << pos);
    *x |= (val & mask) << pos;
}

uint32_t CalculateParity(uint32_t v)
{
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v &= 0xf;
    return (0x6996 >> v) & 1;
}


uint64_t RoundUpToPowerOfTwo(uint32_t value)
{
    if (value > 0)
    {
        int32_t leading = GetLeadingBit(value);
        uint32_t mask = (1 << leading);
        if ((value & ~mask) == 0)
        {
            // value is a power of two
            return static_cast<uint64_t>(value);
        }
        else
        {
            // round up to a power of two
            return (static_cast<uint64_t>(mask) << 1);
        }

    }
    else
    {
        return GE_U64(1);
    }
}

uint32_t RoundDownToPowerOfTwo(uint32_t value)
{
    if (value > 0)
    {
        int32_t leading = GetLeadingBit(value);
        uint32_t mask = (1 << leading);
        return mask;
    }
    else
    {
        return 0;
    }
}

/*
    64-bit unaligned version of MurmurHash
*/
uint64_t MurmurHash64ua(const void* key, int len, unsigned int seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    uint64_t h = seed ^ (len * m);

    const uint8_t* data = (const uint8_t *)key;
    const uint8_t* end = data + (len / 8) * 8;

    while (data != end) 
    {
        uint64_t k;
        std::memcpy(&k, data, sizeof(uint64_t));
        data += sizeof(uint64_t);

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char *data2 = (const unsigned char *)data;
    switch (len & 7) 
    {
        case 7: h ^= (uint64_t)data2[6] << 48;
        case 6: h ^= (uint64_t)data2[5] << 40;
        case 5: h ^= (uint64_t)data2[4] << 32;
        case 4: h ^= (uint64_t)data2[3] << 24;
        case 3: h ^= (uint64_t)data2[2] << 16;
        case 2: h ^= (uint64_t)data2[1] << 8;
        case 1: h ^= (uint64_t)data2[0];
            h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}