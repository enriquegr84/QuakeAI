/* SHA1.cpp

Copyright (c) 2005 Michael D. Leonhard

http://tamale.net/

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "SHA1.h"

#include "Core/Logger/Logger.h"


// print out memory in hexadecimal
void SHA1::HexPrinter( unsigned char* c, int l )
{
	LogAssert( c, "invalid char" );
    LogAssert( l > 0, "invalid length" );
	while( l > 0 )
	{
		printf( " %02x", *c );
		l--;
		c++;
	}
}

// circular left bit rotation.  MSB wraps around to LSB
unsigned int SHA1::LRot( unsigned int x, int bits )
{
	return (x<<bits) | (x>>(32 - bits));
};

// Save a 32-bit unsigned integer to memory, in big-endian order
void SHA1::StoreBigEndianUInt(unsigned char* byte, unsigned int num )
{
	LogAssert( byte, "invalid byte" );
	byte[0] = (unsigned char)(num>>24);
	byte[1] = (unsigned char)(num>>16);
	byte[2] = (unsigned char)(num>>8);
	byte[3] = (unsigned char)num;
}


// Constructor *******************************************************
SHA1::SHA1()
{
	// make sure that the data type is the right size
    LogAssert( sizeof( unsigned int ) * 5 == 20, "invalid data type" );
}

// Destructor ********************************************************
SHA1::~SHA1()
{
	// erase data
	H0 = H1 = H2 = H3 = H4 = 0;
	for( int c = 0; c < 64; c++ ) mBytes[c] = 0;
	mUnprocessedBytes = mSize = 0;
}

// process ***********************************************************
void SHA1::Process()
{
	LogAssert( mUnprocessedBytes == 64, "invalid unprocessed bytes" );
	//printf( "process: " ); hexPrinter( bytes, 64 ); printf( "\n" );
	int t;
	unsigned int a, b, c, d, e, K, f, W[80];
	// starting values
	a = H0;
	b = H1;
	c = H2;
	d = H3;
	e = H4;
	// copy and expand the message block
	for( t = 0; t < 16; t++ ) W[t] = (mBytes[t*4] << 24) + 
        (mBytes[t*4 + 1] << 16) + (mBytes[t*4 + 2] << 8) + mBytes[t*4 + 3];
	for(; t< 80; t++ ) W[t] = LRot( W[t-3]^W[t-8]^W[t-14]^W[t-16], 1 );

	/* main loop */
	unsigned int temp;
	for( t = 0; t < 80; t++ )
	{
		if( t < 20 ) 
        {
			K = 0x5a827999;
			f = (b & c) | ((b ^ 0xFFFFFFFF) & d);//TODO: try using ~
		} 
        else if( t < 40 ) 
        {
			K = 0x6ed9eba1;
			f = b ^ c ^ d;
		} 
        else if( t < 60 ) 
        {
			K = 0x8f1bbcdc;
			f = (b & c) | (b & d) | (c & d);
		} 
        else 
        {
			K = 0xca62c1d6;
			f = b ^ c ^ d;
		}
		temp = LRot(a,5) + f + e + W[t] + K;
		e = d;
		d = c;
		c = LRot(b,30);
		b = a;
		a = temp;
		//printf( "t=%d %08x %08x %08x %08x %08x\n",t,a,b,c,d,e );
	}
	/* add variables */
	H0 += a;
	H1 += b;
	H2 += c;
	H3 += d;
	H4 += e;
	//printf( "Current: %08x %08x %08x %08x %08x\n",H0,H1,H2,H3,H4 );
	/* all bytes have been processed */
	mUnprocessedBytes = 0;
}

// AddBytes **********************************************************
void SHA1::AddBytes( const char* data, int num )
{
	LogAssert( data, "invalid data" );
    LogAssert( num >= 0, "invalid number of bytes" );
	// add these bytes to the running total
	mSize += num;
	// repeat until all data is processed
	while( num > 0 )
	{
		// number of bytes required to complete block
		int needed = 64 - mUnprocessedBytes;
		LogAssert( needed > 0, "invalid number of bytes required" );
		// number of bytes to copy (use smaller of two)
		int toCopy = (num < needed) ? num : needed;
		// Copy the bytes
		memcpy( mBytes + mUnprocessedBytes, data, toCopy );
		// Bytes have been copied
		num -= toCopy;
		data += toCopy;
		mUnprocessedBytes += toCopy;

		// there is a full block
		if( mUnprocessedBytes == 64 ) Process();
	}
}

// digest ************************************************************
unsigned char* SHA1::GetDigest()
{
	// save the message size
	unsigned int totalBitsL = mSize << 3;
	unsigned int totalBitsH = mSize >> 29;
	// add 0x80 to the message
	AddBytes( "\x80", 1 );

	unsigned char footer[64] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	// block has no room for 8-byte filesize, so finish it
	if( mUnprocessedBytes > 56 )
		AddBytes( (char*)footer, 64 - mUnprocessedBytes);
	LogAssert( mUnprocessedBytes <= 56, "invalid unprocessed bytes" );
	// how many zeros do we need
	int neededZeros = 56 - mUnprocessedBytes;
	// store file size (in bits) in big-endian format
	StoreBigEndianUInt( footer + neededZeros, totalBitsH );
	StoreBigEndianUInt( footer + neededZeros + 4, totalBitsL );
	// finish the final block
	AddBytes( (char*)footer, neededZeros + 8 );
	// allocate memory for the digest bytes
	unsigned char* digest = (unsigned char*)malloc( 20 );
	// copy the digest bytes
	StoreBigEndianUInt( digest, H0 );
	StoreBigEndianUInt( digest + 4, H1 );
	StoreBigEndianUInt( digest + 8, H2 );
	StoreBigEndianUInt( digest + 12, H3 );
	StoreBigEndianUInt( digest + 16, H4 );
	// return the digest
	return digest;
}
