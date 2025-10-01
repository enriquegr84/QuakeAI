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

#include "Serialize.h"

#include <iomanip>

#include <zlib/zlib.h>

FloatType SerializeFloatType = FLOATTYPE_UNKNOWN;


/* report a zlib or i/o error */
void ZErr(int ret)
{
    switch (ret) 
    {
        case Z_ERRNO:
            if (ferror(stdin))
                LogError("error reading stdin");
            else if (ferror(stdout))
                LogError("error writing stdout");
            break;
        case Z_STREAM_ERROR:
            LogError("invalid compression level");
            break;
        case Z_DATA_ERROR:
            LogError("invalid or incomplete deflate data");
            break;
        case Z_MEM_ERROR:
            LogError("out of memory");
            break;
        case Z_VERSION_ERROR:
            LogError("zlib version mismatch!");
            break;
        default:
            LogError("return value = " + ret);
    }
}

void CompressZlib(const unsigned char* data, size_t dataSize, std::ostream& os, int level)
{
    z_stream z;
    const int bufSize = 16384;
    char outputBuffer[bufSize];
    int status = 0;
    int ret;

    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;

    ret = deflateInit(&z, level);
    if (ret != Z_OK)
        throw SerializationError("compressZlib: deflateInit failed");

    // Point zlib to our input buffer
    z.next_in = (Bytef*)&data[0];
    z.avail_in = (uInt)dataSize;
    // And get all output
    for (;;)
    {
        z.next_out = (Bytef*)outputBuffer;
        z.avail_out = bufSize;

        status = deflate(&z, Z_FINISH);
        if (status == Z_NEED_DICT || status == Z_DATA_ERROR || status == Z_MEM_ERROR)
        {
            ZErr(status);
            throw SerializationError("compressZlib: deflate failed");
        }
        int count = bufSize - z.avail_out;
        if (count)
            os.write(outputBuffer, count);
        // This determines zlib has given all output
        if (status == Z_STREAM_END)
            break;
    }

    deflateEnd(&z);
}

void CompressZlib(const std::string& data, std::ostream& os, int level)
{
    CompressZlib((unsigned char*)data.c_str(), data.size(), os, level);
}

void DecompressZlib(std::istream& is, std::ostream& os, size_t limit)
{
    z_stream z;
    const int bufSize = 16384;
    char inputBuffer[bufSize];
    char outputBuffer[bufSize];
    int status = 0;
    int ret;
    int bytesRead = 0;
    int bytesWritten = 0;
    int inputBufferLen = 0;

    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;

    ret = inflateInit(&z);
    if (ret != Z_OK)
        throw SerializationError("dcompressZlib: inflateInit failed");

    z.avail_in = 0;

    //dstream<<"initial fail="<<is.fail()<<" bad="<<is.bad()<<std::endl;

    for (;;)
    {
        int outputSize = bufSize;
        z.next_out = (Bytef*)outputBuffer;
        z.avail_out = outputSize;

        if (limit) 
        {
            int limitRemaining = (uInt)limit - bytesWritten;
            if (limitRemaining <= 0) 
            {
                // we're aborting ahead of time - throw an error?
                break;
            }
            if (limitRemaining < outputSize) 
            {
                z.avail_out = outputSize = limitRemaining;
            }
        }

        if (z.avail_in == 0)
        {
            z.next_in = (Bytef*)inputBuffer;
            is.read(inputBuffer, bufSize);
            inputBufferLen = (int)is.gcount();
            z.avail_in = inputBufferLen;
            //dstream<<"read fail="<<is.fail()<<" bad="<<is.bad()<<std::endl;
        }
        if (z.avail_in == 0)
        {
            //dstream<<"z.avail_in == 0"<<std::endl;
            break;
        }

        //dstream<<"1 z.avail_in="<<z.avail_in<<std::endl;
        status = inflate(&z, Z_NO_FLUSH);
        //dstream<<"2 z.avail_in="<<z.avail_in<<std::endl;
        bytesRead += (int)(is.gcount() - z.avail_in);
        //dstream<<"bytes_read="<<bytes_read<<std::endl;

        if (status == Z_NEED_DICT || status == Z_DATA_ERROR || status == Z_MEM_ERROR)
        {
            ZErr(status);
            throw SerializationError("decompressZlib: inflate failed");
        }
        int count = outputSize - z.avail_out;
        //dstream<<"count="<<count<<std::endl;
        if (count)
            os.write(outputBuffer, count);
        bytesWritten += count;
        if (status == Z_STREAM_END)
        {
            //dstream<<"Z_STREAM_END"<<std::endl;

            //dstream<<"z.avail_in="<<z.avail_in<<std::endl;
            //dstream<<"fail="<<is.fail()<<" bad="<<is.bad()<<std::endl;
            // Unget all the data that inflate didn't take
            is.clear(); // Just in case EOF is set
            for (unsigned int i = 0; i < z.avail_in; i++)
            {
                is.unget();
                if (is.fail() || is.bad())
                {
                    LogError("unget #" + std::to_string(i) + " failed" 
                        "\nfail=" + std::to_string(is.fail()) + 
                        " bad=" + std::to_string(is.bad()));
                    throw SerializationError("decompressZlib: unget failed");
                }
            }

            break;
        }
    }

    inflateEnd(&z);
}

void Decompress(std::istream& is, std::ostream& os, unsigned char version)
{
    if (version >= 11)
    {
        DecompressZlib(is, os);
        return;
    }

    // Read length (u32)

    unsigned char tmp[4];
    is.read((char*)tmp, 4);
    unsigned int len = ReadUInt32(tmp);

    // We will be reading 8-bit pairs of more_count and byte
    unsigned int count = 0;
    for (;;)
    {
        unsigned char moreCount = 0;
        unsigned char byte = 0;

        is.read((char*)&moreCount, 1);

        is.read((char*)&byte, 1);

        if (is.eof())
            throw SerializationError("decompress: stream ended halfway");

        for (int i = 0; i < (unsigned short)moreCount + 1; i++)
            os.write((char*)&byte, 1);

        count += (unsigned short)moreCount + 1;

        if (count == len)
            break;
    }
}


////
//// String
////

std::string SerializeString16(const std::string& plain)
{
	std::string str;
    char buf[2];

	if (plain.size() > STRING_MAX_LEN)
		throw SerializationError("String too long for serializeString16");
    str.reserve(2 + plain.size());

	WriteUInt16((uint8_t*)&buf[0], (unsigned short)plain.size());
    str.append(buf, 2);

    str.append(plain);
	return str;
}

std::string DeserializeString16(std::istream& is)
{
	std::string str;
	char buf[2];

	is.read(buf, 2);
	if (is.gcount() != 2)
		throw SerializationError("DeserializeString16: size not read");

	unsigned short strSize = ReadUInt16((uint8_t*)buf);
	if (strSize == 0)
		return str;

    str.resize(strSize);
	is.read(&str[0], strSize);
	if (is.gcount() != strSize)
		throw SerializationError("DeserializeString16: couldn't read all chars");

	return str;
}


////
//// Long String
////

std::string SerializeString32(const std::string& plain)
{
	std::string str;
	char buf[4];

	if (plain.size() > LONG_STRING_MAX_LEN)
		throw SerializationError("String too long for serializeLongString");
    str.reserve(4 + plain.size());

	WriteUInt32((uint8_t*)&buf[0], (unsigned int)plain.size());
    str.append(buf, 4);
    str.append(plain);
	return str;
}

std::string DeserializeString32(std::istream& is)
{
	std::string str;
	char buf[4];

	is.read(buf, 4);
	if (is.gcount() != 4)
		throw SerializationError("DeserializeLongString: size not read");

	unsigned int strSize = ReadUInt32((unsigned char*)buf);
	if (strSize == 0)
		return str;

	// We don't really want a remote attacker to force us to allocate 4GB...
	if (strSize > LONG_STRING_MAX_LEN) 
		throw SerializationError("DeserializeLongString: string too long: " + 
            std::to_string(strSize) + " bytes");

    str.resize(strSize);
	is.read(&str[0], strSize);
	if ((unsigned int)is.gcount() != strSize)
		throw SerializationError("DeserializeLongString: couldn't read all chars");

	return str;
}

////
//// JSON
////

std::string SerializeJsonString(const std::string& plain)
{
	std::ostringstream os(std::ios::binary);
	os << "\"";

	for (char c : plain) 
    {
		switch (c) 
        {
			case '"':
				os << "\\\"";
				break;
			case '\\':
				os << "\\\\";
				break;
			case '/':
				os << "\\/";
				break;
			case '\b':
				os << "\\b";
				break;
			case '\f':
				os << "\\f";
				break;
			case '\n':
				os << "\\n";
				break;
			case '\r':
				os << "\\r";
				break;
			case '\t':
				os << "\\t";
				break;
			default: {
				if (c >= 32 && c <= 126) 
                {
					os << c;
				} 
                else
                {
					unsigned int cnum = (unsigned char)c;
					os << "\\u" << std::hex << std::setw(4)
						<< std::setfill('0') << cnum;
				}
				break;
			}
		}
	}

	os << "\"";
	return os.str();
}

std::string DeserializeJsonString(std::istream& is)
{
	std::ostringstream os(std::ios::binary);
	char c, c2;

	// Parse initial doublequote
	is >> c;
	if (c != '"')
		throw SerializationError("JSON string must start with doublequote");

	// Parse characters
	for (;;) 
    {
		c = is.get();
		if (is.eof())
			throw SerializationError("JSON string ended prematurely");

		if (c == '"') 
        {
			return os.str();
		}

		if (c == '\\') 
        {
			c2 = is.get();
			if (is.eof())
				throw SerializationError("JSON string ended prematurely");
			switch (c2) 
            {
				case 'b':
					os << '\b';
					break;
				case 'f':
					os << '\f';
					break;
				case 'n':
					os << '\n';
					break;
				case 'r':
					os << '\r';
					break;
				case 't':
					os << '\t';
					break;
				case 'u': 
                {
					int hexnumber;
					char hexdigits[4 + 1];

					is.read(hexdigits, 4);
					if (is.eof())
						throw SerializationError("JSON string ended prematurely");
					hexdigits[4] = 0;

					std::istringstream tmp_is(hexdigits, std::ios::binary);
					tmp_is >> std::hex >> hexnumber;
					os << (char)hexnumber;
					break;
				}
				default:
					os << c2;
					break;
			}
		} 
        else 
        {
			os << c;
		}
	}

	return os.str();
}

std::string SerializeJsonStringIfNeeded(const std::string& s)
{
	for (size_t i = 0; i < s.size(); ++i) 
		if (s[i] <= 0x1f || s[i] >= 0x7f || s[i] == ' ' || s[i] == '\"')
			return SerializeJsonString(s);

	return s;
}

std::string DeserializeJsonStringIfNeeded(std::istream& is)
{
	std::ostringstream tmpOs;
	bool expectInitialQuote = true;
	bool isJSon = false;
	bool wasBackslash = false;
	for (;;) 
    {
		char c = is.get();
		if (is.eof())
			break;

		if (expectInitialQuote && c == '"') 
        {
            tmpOs << c;
            isJSon = true;
		} 
        else if(isJSon)
        {
            tmpOs << c;
			if (wasBackslash)
                wasBackslash = false;
			else if (c == '\\')
                wasBackslash = true;
			else if (c == '"')
				break; // Found end of string
		} 
        else 
        {
			if (c == ' ') 
            {
				// Found end of word
				is.unget();
				break;
			}

            tmpOs << c;
		}
        expectInitialQuote = false;
	}
	if (isJSon) 
    {
		std::istringstream tmp_is(tmpOs.str(), std::ios::binary);
		return DeserializeJsonString(tmp_is);
	}

	return tmpOs.str();
}

