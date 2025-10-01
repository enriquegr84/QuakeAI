/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"
#include "Mathematic/Arithmetic/IEEEFloat.h"

#include "Graphic/Resource/Color.h"

#include "Core/Logger/Logger.h"



#define FIXEDPOINT_FACTOR 1000.0f

// 0x7FFFFFFF / 1000.0f is not serializable.
// The limited float precision at this magnitude may cause the result to round
// to a greater value than can be represented by a 32 bit integer when increased
// by a factor of FIXEDPOINT_FACTOR.  As a result, [F1000_MIN..F1000_MAX] does
// not represent the full range, but rather the largest safe range, of values on
// all supported architectures.  Note: This definition makes assumptions on
// platform float-to-int conversion behavior.
#define FLOAT_MIN ((float)(int32_t)((float)(-0x7FFFFFFF - 1) / FIXEDPOINT_FACTOR))
#define FLOAT_MAX ((float)(int32_t)((float)(0x7FFFFFFF) / FIXEDPOINT_FACTOR))

#define STRING_MAX_LEN 0xFFFF
#define WIDE_STRING_MAX_LEN 0xFFFF
// 64 MB ought to be enough for anybody - Billy G.
#define LONG_STRING_MAX_LEN (64 * 1024 * 1024)

extern FloatType SerializeFloatType;

// This represents an uninitialized or invalid format
#define SER_FMT_VER_INVALID 255
// Highest supported serialization version
#define SER_FMT_VER_HIGHEST_READ 28
// Saved on disk version
#define SER_FMT_VER_HIGHEST_WRITE 28
// Lowest supported serialization version
#define SER_FMT_VER_LOWEST_READ 0
// Lowest serialization version for writing
// Can't do < 24 anymore; we have 16-bit dynamically allocated node IDs
// in memory; conversion just won't work in this direction.
#define SER_FMT_VER_LOWEST_WRITE 24

inline bool VersionSupported(int version)
{
    return version >= SER_FMT_VER_LOWEST_READ && version <= SER_FMT_VER_HIGHEST_READ;
}

/*
    Misc. serialization functions
*/
void CompressZlib(const unsigned char* data, size_t dataSize, std::ostream& os, int level = -1);
void CompressZlib(const std::string& data, std::ostream& os, int level = -1);
void DecompressZlib(std::istream& is, std::ostream& os, size_t limit = 0);

//void compress(const std::string &data, std::ostream &os, u8 version);
void Decompress(std::istream& is, std::ostream& os, unsigned char version);

// generic byte-swapping implementation

inline unsigned short ReadUInt16(const uint8_t* data)
{
	return
		((uint16_t)data[0] << 8) | ((uint16_t)data[1] << 0);
}

inline uint32_t ReadUInt32(const uint8_t* data)
{
	return
		((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		((uint32_t)data[2] <<  8) | ((uint32_t)data[3] <<  0);
}

inline uint64_t ReadUInt64(const uint8_t* data)
{
	return
		((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) |
		((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32) |
		((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) |
		((uint64_t)data[6] <<  8) | ((uint64_t)data[7] << 0);
}

inline void WriteUInt16(uint8_t* data, unsigned short i)
{
	data[0] = (i >> 8) & 0xFF;
	data[1] = (i >> 0) & 0xFF;
}

inline void WriteUInt32(uint8_t* data, uint32_t i)
{
	data[0] = (i >> 24) & 0xFF;
	data[1] = (i >> 16) & 0xFF;
	data[2] = (i >>  8) & 0xFF;
	data[3] = (i >>  0) & 0xFF;
}

inline void WriteUInt64(uint8_t* data, uint64_t i)
{
	data[0] = (i >> 56) & 0xFF;
	data[1] = (i >> 48) & 0xFF;
	data[2] = (i >> 40) & 0xFF;
	data[3] = (i >> 32) & 0xFF;
	data[4] = (i >> 24) & 0xFF;
	data[5] = (i >> 16) & 0xFF;
	data[6] = (i >>  8) & 0xFF;
	data[7] = (i >>  0) & 0xFF;
}

//////////////// read routines ////////////////

inline uint8_t ReadUInt8(const uint8_t* data)
{
	return ((uint8_t)data[0] << 0);
}

inline int8_t ReadInt8(const uint8_t* data)
{
	return (int8_t)ReadUInt8(data);
}

inline int16_t ReadInt16(const uint8_t* data)
{
	return (int16_t)ReadUInt16(data);
}

inline int32_t ReadInt32(const uint8_t* data)
{
	return (int32_t)ReadUInt32(data);
}

inline int64_t ReadInt64(const uint8_t* data)
{
	return (int64_t)ReadUInt64(data);
}

inline float ReadFloat(const uint8_t* data)
{
	uint32_t u = ReadUInt32(data);

	switch (SerializeFloatType) 
    {
	    case FLOATTYPE_SYSTEM: 
        {
			float f;
			memcpy(&f, &u, 4);
			return f;
        }
	    case FLOATTYPE_SLOW:
		    return UInt32ToFloatSlow(u);
	    case FLOATTYPE_UNKNOWN: // First initialization
		    SerializeFloatType = GetFloatSerializationType();
		    return ReadFloat(data);
	}
	throw SerializationError("ReadFloat: Unreachable code");
}

inline SColor ReadARGB8(const uint8_t* data)
{
	SColor p(ReadUInt32(data));
	return p;
}

inline Vector2<short> ReadV2Short(const uint8_t* data)
{
	Vector2<short> p;
	p[0] = ReadInt16(&data[0]);
	p[1] = ReadInt16(&data[2]);
	return p;
}

inline Vector3<short> ReadV3Short(const uint8_t* data)
{
	Vector3<short> p;
	p[0] = ReadInt16(&data[0]);
	p[1] = ReadInt16(&data[2]);
	p[2] = ReadInt16(&data[4]);
	return p;
}

inline Vector2<int> ReadV2Int(const uint8_t* data)
{
	Vector2<int> p;
	p[0] = ReadInt32(&data[0]);
	p[1] = ReadInt32(&data[4]);
	return p;
}

inline Vector3<int> ReadV3Int(const uint8_t* data)
{
	Vector3<int> p;
	p[0] = ReadInt32(&data[0]);
	p[1] = ReadInt32(&data[4]);
	p[2] = ReadInt32(&data[8]);
	return p;
}

inline Vector2<float> ReadV2Float(const uint8_t* data)
{
	Vector2<float> p;
	p[0] = ReadFloat(&data[0]);
	p[1] = ReadFloat(&data[4]);
	return p;
}

inline Vector3<float> ReadV3Float(const uint8_t* data)
{
	Vector3<float> p;
	p[0] = ReadFloat(&data[0]);
	p[1] = ReadFloat(&data[4]);
	p[2] = ReadFloat(&data[8]);
	return p;
}

/////////////// write routines ////////////////

inline void WriteUInt8(uint8_t* data, uint8_t i)
{
	data[0] = (i >> 0) & 0xFF;
}

inline void WriteInt8(uint8_t* data, int8_t i)
{
	WriteUInt8(data, (uint8_t)i);
}

inline void WriteInt16(uint8_t* data, int16_t i)
{
	WriteUInt16(data, (uint16_t)i);
}

inline void WriteInt32(uint8_t* data, int32_t i)
{
	WriteUInt32(data, (uint32_t)i);
}

inline void WriteInt64(uint8_t* data, int64_t i)
{
	WriteUInt64(data, (uint64_t)i);
}

inline void WriteFloat(uint8_t* data, float i)
{
	switch (SerializeFloatType) 
    {
	    case FLOATTYPE_SYSTEM: 
        {
			uint32_t u;
			memcpy(&u, &i, 4);
			return WriteUInt32(data, u);
		}
	    case FLOATTYPE_SLOW:
		    return WriteUInt32(data, FloatToUInt32Slow(i));
	    case FLOATTYPE_UNKNOWN: // First initialization
		    SerializeFloatType = GetFloatSerializationType();
		    return WriteFloat(data, i);
	}
	throw SerializationError("WriteFloat: Unreachable code");
}

inline void WriteARGB8(uint8_t* data, SColor p)
{
	WriteUInt32(data, p.mColor);
}

inline void WriteV2Short(uint8_t* data, Vector2<short> p)
{
	WriteInt16(&data[0], p[0]);
	WriteInt16(&data[2], p[1]);
}

inline void WriteV3Short(uint8_t* data, Vector3<short> p)
{
	WriteInt16(&data[0], p[0]);
	WriteInt16(&data[2], p[1]);
	WriteInt16(&data[4], p[2]);
}

inline void WriteV2Int(uint8_t* data, Vector2<int> p)
{
	WriteInt32(&data[0], p[0]);
	WriteInt32(&data[4], p[1]);
}

inline void WriteV3Int(uint8_t* data, Vector3<int> p)
{
	WriteInt32(&data[0], p[0]);
	WriteInt32(&data[4], p[1]);
	WriteInt32(&data[8], p[2]);
}

inline void WriteV2Float(uint8_t* data, Vector2<float> p)
{
	WriteFloat(&data[0], p[0]);
	WriteFloat(&data[4], p[1]);
}

inline void WriteV3Float(uint8_t* data, Vector3<float> p)
{
	WriteFloat(&data[0], p[0]);
	WriteFloat(&data[4], p[1]);
	WriteFloat(&data[8], p[2]);
}


////
//// Iostream wrapper for data read/write
////

#define MAKE_STREAM_READ_FXN(T, N, S)    \
	inline T Read ## N(std::istream &is) \
	{                                    \
		char buf[S] = {0};               \
		is.read(buf, sizeof(buf));       \
		return Read ## N((uint8_t *)buf);\
	}

#define MAKE_STREAM_WRITE_FXN(T, N, S)              \
	inline void Write ## N(std::ostream &os, T val) \
	{                                               \
		char buf[S];                                \
		Write ## N((uint8_t *)buf, val);            \
		os.write(buf, sizeof(buf));                 \
	}

MAKE_STREAM_READ_FXN(uint8_t, UInt8, 1);
MAKE_STREAM_READ_FXN(uint16_t, UInt16, 2);
MAKE_STREAM_READ_FXN(uint32_t, UInt32, 4);
MAKE_STREAM_READ_FXN(uint64_t, UInt64, 8);
MAKE_STREAM_READ_FXN(int8_t, Int8, 1);
MAKE_STREAM_READ_FXN(int16_t, Int16, 2);
MAKE_STREAM_READ_FXN(int32_t, Int32, 4);
MAKE_STREAM_READ_FXN(int64_t, Int64, 8);
MAKE_STREAM_READ_FXN(float, Float, 4);
MAKE_STREAM_READ_FXN(Vector2<short>, V2Short, 4);
MAKE_STREAM_READ_FXN(Vector3<short>, V3Short, 6);
MAKE_STREAM_READ_FXN(Vector2<int>, V2Int, 8);
MAKE_STREAM_READ_FXN(Vector3<int>, V3Int, 12);
MAKE_STREAM_READ_FXN(Vector3<float>, V3Float, 12);
MAKE_STREAM_READ_FXN(Vector2<float>, V2Float, 8);
MAKE_STREAM_READ_FXN(SColor, ARGB8, 4);

MAKE_STREAM_WRITE_FXN(uint8_t, UInt8, 1);
MAKE_STREAM_WRITE_FXN(uint16_t, UInt16, 2);
MAKE_STREAM_WRITE_FXN(uint32_t, UInt32, 4);
MAKE_STREAM_WRITE_FXN(uint64_t, UInt64, 8);
MAKE_STREAM_WRITE_FXN(int8_t, Int8, 1);
MAKE_STREAM_WRITE_FXN(int16_t, Int16, 2);
MAKE_STREAM_WRITE_FXN(int32_t, Int32, 4);
MAKE_STREAM_WRITE_FXN(int64_t, Int64, 8);
MAKE_STREAM_WRITE_FXN(float, Float, 4);
MAKE_STREAM_WRITE_FXN(Vector2<short>, V2Short, 4);
MAKE_STREAM_WRITE_FXN(Vector3<short>, V3Short, 6);
MAKE_STREAM_WRITE_FXN(Vector2<int>, V2Int, 8);
MAKE_STREAM_WRITE_FXN(Vector3<int>, V3Int, 12);
MAKE_STREAM_WRITE_FXN(Vector3<float>, V3Float, 12);
MAKE_STREAM_WRITE_FXN(Vector2<float>, V2Float, 8);
MAKE_STREAM_WRITE_FXN(SColor, ARGB8, 4);


////
//// More serialization stuff
////

// Creates a string with the length as the first two bytes
std::string SerializeString16(const std::string& plain);

// Reads a string with the length as the first two bytes
std::string DeserializeString16(std::istream& is);

// Creates a string with the length as the first four bytes
std::string SerializeString32(const std::string& plain);

// Reads a string with the length as the first four bytes
std::string DeserializeString32(std::istream& is);

// Creates a string encoded in JSON format (almost equivalent to a C string literal)
std::string SerializeJsonString(const std::string& plain);

// Reads a string encoded in JSON format
std::string DeserializeJsonString(std::istream& is);

// If the string contains spaces, quotes or control characters, encodes as JSON.
// Else returns the string unmodified.
std::string SerializeJsonStringIfNeeded(const std::string& s);

// Parses a string serialized by serializeJsonStringIfNeeded.
std::string DeserializeJsonStringIfNeeded(std::istream& is);

#endif
