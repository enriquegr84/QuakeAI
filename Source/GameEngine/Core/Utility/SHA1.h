/* SHA1.h

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

#ifndef SHA1_H
#define SHA1_H

class SHA1
{
private:
	// fields
    unsigned int H0 = 0x67452301;
    unsigned int H1 = 0xefcdab89;
    unsigned int H2 = 0x98badcfe;
    unsigned int H3 = 0x10325476;
    unsigned int H4 = 0xc3d2e1f0;
	unsigned char mBytes[64];
	int mUnprocessedBytes = 0;
    unsigned int mSize = 0;
	void Process();

public:
	SHA1();
	~SHA1();
	void AddBytes(const char *data, int num);
	unsigned char* GetDigest();
	// utility methods
	static unsigned int LRot(unsigned int x, int bits);
	static void StoreBigEndianUInt(unsigned char *byte, unsigned int num);
	static void HexPrinter(unsigned char *c, int l);
};

#endif