/*
 * Minetest
 * Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>
 * Copyright (C) 2010-2014 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Noise.h"

#include "Util.h"

#include "Core/Logger/Logger.h"

#include "Mathematic/Function/Functions.h"

#define NOISE_MAGIC_X    1619
#define NOISE_MAGIC_Y    31337
#define NOISE_MAGIC_Z    52591
#define NOISE_MAGIC_SEED 1013

typedef float (*Interp2dFxn)(
		float v00, float v10, float v01, float v11,
		float x, float y);

typedef float (*Interp3dFxn)(
		float v000, float v100, float v010, float v110,
		float v001, float v101, float v011, float v111,
		float x, float y, float z);

FlagDescription FlagdescNoiseparams[] = {
	{"defaults",    NOISE_FLAG_DEFAULTS},
	{"eased",       NOISE_FLAG_EASED},
	{"absvalue",    NOISE_FLAG_ABSVALUE},
	{"pointbuffer", NOISE_FLAG_POINTBUFFER},
	{"simplex",     NOISE_FLAG_SIMPLEX},
	{NULL,          0}
};

float EaseCurve(float t)
{
    return t * t * t * (t * (6.f * t - 15.f) + 10.f);
}

float Noise2d(int x, int y, int seed)
{
	unsigned int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y
			+ NOISE_MAGIC_SEED * seed) & 0x7fffffff;
	n = (n >> 13) ^ n;
	n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 1.f - (float)(int)n / 0x40000000;
}


float Noise3d(int x, int y, int z, int seed)
{
	unsigned int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y + NOISE_MAGIC_Z * z
			+ NOISE_MAGIC_SEED * seed) & 0x7fffffff;
	n = (n >> 13) ^ n;
	n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 1.f - (float)(int)n / 0x40000000;
}


inline float DotProduct(float vx, float vy, float wx, float wy)
{
	return vx * wx + vy * wy;
}


inline float LinearInterpolation(float v0, float v1, float t)
{
	return v0 + (v1 - v0) * t;
}

inline float BiLinearInterpolation(
	float v00, float v10,
	float v01, float v11,
	float x, float y)
{
	float tx = EaseCurve(x);
	float ty = EaseCurve(y);
	float u = LinearInterpolation(v00, v10, tx);
	float v = LinearInterpolation(v01, v11, tx);
	return LinearInterpolation(u, v, ty);
}


inline float BiLinearInterpolationNoEase(
	float v00, float v10,
	float v01, float v11,
	float x, float y)
{
	float u = LinearInterpolation(v00, v10, x);
	float v = LinearInterpolation(v01, v11, x);
	return LinearInterpolation(u, v, y);
}


float TriLinearInterpolation(
	float v000, float v100, float v010, float v110,
	float v001, float v101, float v011, float v111,
	float x, float y, float z)
{
	float tx = EaseCurve(x);
	float ty = EaseCurve(y);
	float tz = EaseCurve(z);
	float u = BiLinearInterpolationNoEase(v000, v100, v010, v110, tx, ty);
	float v = BiLinearInterpolationNoEase(v001, v101, v011, v111, tx, ty);
	return LinearInterpolation(u, v, tz);
}

float TriLinearInterpolationNoEase(
	float v000, float v100, float v010, float v110,
	float v001, float v101, float v011, float v111,
	float x, float y, float z)
{
	float u = BiLinearInterpolationNoEase(v000, v100, v010, v110, x, y);
	float v = BiLinearInterpolationNoEase(v001, v101, v011, v111, x, y);
	return LinearInterpolation(u, v, z);
}

float Noise2dGradient(float x, float y, int seed, bool eased)
{
	// Calculate the integer coordinates
	int x0 = (int)Function<float>::Floor(x);
	int y0 = (int)Function<float>::Floor(y);
	// Calculate the remaining part of the coordinates
	float xl = x - (float)x0;
	float yl = y - (float)y0;
	// Get values for corners of square
	float v00 = Noise2d(x0, y0, seed);
	float v10 = Noise2d(x0+1, y0, seed);
	float v01 = Noise2d(x0, y0+1, seed);
	float v11 = Noise2d(x0+1, y0+1, seed);
	// Interpolate
	if (eased)
		return BiLinearInterpolation(v00, v10, v01, v11, xl, yl);

	return BiLinearInterpolationNoEase(v00, v10, v01, v11, xl, yl);
}


float Noise3dGradient(float x, float y, float z, int seed, bool eased)
{
	// Calculate the integer coordinates
	int x0 = (int)Function<float>::Floor(x);
	int y0 = (int)Function<float>::Floor(y);
	int z0 = (int)Function<float>::Floor(z);
	// Calculate the remaining part of the coordinates
	float xl = x - (float)x0;
	float yl = y - (float)y0;
	float zl = z - (float)z0;
	// Get values for corners of cube
	float v000 = Noise3d(x0,     y0,     z0,     seed);
	float v100 = Noise3d(x0 + 1, y0,     z0,     seed);
	float v010 = Noise3d(x0,     y0 + 1, z0,     seed);
	float v110 = Noise3d(x0 + 1, y0 + 1, z0,     seed);
	float v001 = Noise3d(x0,     y0,     z0 + 1, seed);
	float v101 = Noise3d(x0 + 1, y0,     z0 + 1, seed);
	float v011 = Noise3d(x0,     y0 + 1, z0 + 1, seed);
	float v111 = Noise3d(x0 + 1, y0 + 1, z0 + 1, seed);
	// Interpolate
	if (eased) 
    {
		return TriLinearInterpolation(
			v000, v100, v010, v110,
			v001, v101, v011, v111,
			xl, yl, zl);
	}

	return TriLinearInterpolationNoEase(
		v000, v100, v010, v110,
		v001, v101, v011, v111,
		xl, yl, zl);
}


float Noise2dPerlin(float x, float y, int seed,
	int octaves, float persistence, bool eased)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;
	for (int i = 0; i < octaves; i++)
	{
		a += g * Noise2dGradient(x * f, y * f, seed + i, eased);
		f *= 2.0;
		g *= persistence;
	}
	return a;
}


float Noise2dPerlinAbs(float x, float y, int seed,
	int octaves, float persistence, bool eased)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;
	for (int i = 0; i < octaves; i++) 
    {
		a += g * std::fabs(Noise2dGradient(x * f, y * f, seed + i, eased));
		f *= 2.0;
		g *= persistence;
	}
	return a;
}


float Noise3dPerlin(float x, float y, float z, int seed,
	int octaves, float persistence, bool eased)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;
	for (int i = 0; i < octaves; i++) 
    {
		a += g * Noise3dGradient(x * f, y * f, z * f, seed + i, eased);
		f *= 2.0;
		g *= persistence;
	}
	return a;
}


float Noise3dPerlinAbs(float x, float y, float z, int seed,
	int octaves, float persistence, bool eased)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;
	for (int i = 0; i < octaves; i++) 
    {
		a += g * std::fabs(Noise3dGradient(x * f, y * f, z * f, seed + i, eased));
		f *= 2.0;
		g *= persistence;
	}
	return a;
}


float Contour(float v)
{
	v = std::fabs(v);
	if (v >= 1.f)
		return 0.f;
	return (1.f - v);
}


///////////////////////// [ New noise ] ////////////////////////////


float NoisePerlin2D(const NoiseParams* np, float x, float y, int seed)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	x /= np->spread[0];
	y /= np->spread[1];
	seed += np->seed;

	for (uint16_t i = 0; i < np->octaves; i++) 
    {
		float noiseval = Noise2dGradient(x * f, y * f, seed + i,
			np->flags & (NOISE_FLAG_DEFAULTS | NOISE_FLAG_EASED));

		if (np->flags & NOISE_FLAG_ABSVALUE)
			noiseval = std::fabs(noiseval);

		a += g * noiseval;
		f *= np->lacunarity;
		g *= np->persist;
	}

	return np->offset + a * np->scale;
}


float NoisePerlin3D(const NoiseParams* np, float x, float y, float z, int seed)
{
	float a = 0;
	float f = 1.0;
	float g = 1.0;

	x /= np->spread[0];
	y /= np->spread[1];
	z /= np->spread[2];
	seed += np->seed;

	for (uint16_t i = 0; i < np->octaves; i++) 
    {
		float noiseval = Noise3dGradient(x * f, y * f, z * f, seed + i,
			np->flags & NOISE_FLAG_EASED);

		if (np->flags & NOISE_FLAG_ABSVALUE)
			noiseval = std::fabs(noiseval);

		a += g * noiseval;
		f *= np->lacunarity;
		g *= np->persist;
	}

	return np->offset + a * np->scale;
}


Noise::Noise(const NoiseParams* np, int seed, unsigned int sx, unsigned int sy, unsigned int sz)
{
    this->mNoiseParams = *np;
	this->mSeed = seed;
	this->mSizeX = sx;
	this->mSizeY = sy;
	this->mSizeZ = sz;

	AllocBuffers();
}


Noise::~Noise()
{
	delete[] mGradientBuf;
	delete[] mPersistBuf;
	delete[] mNoiseBuf;
	delete[] mResult;
}


void Noise::AllocBuffers()
{
	if (mSizeX < 1)
        mSizeX = 1;
	if (mSizeY < 1)
        mSizeY = 1;
	if (mSizeZ < 1)
        mSizeZ = 1;

	this->mNoiseBuf = NULL;
	ResizeNoiseBuf(mSizeZ > 1);

	delete[] mGradientBuf;
	delete[] mPersistBuf;
	delete[] mResult;

	try
    {
		size_t bufsize = mSizeX * mSizeY * mSizeZ;
		this->mPersistBuf  = NULL;
		this->mGradientBuf = new float[bufsize];
		this->mResult = new float[bufsize];
	} 
    catch (std::bad_alloc&) 
    {
		throw InvalidNoiseParamsException();
	}
}


void Noise::SetSize(unsigned int sx, unsigned int sy, unsigned int sz)
{
	this->mSizeX = sx;
	this->mSizeY = sy;
	this->mSizeZ = sz;

	AllocBuffers();
}


void Noise::SetSpreadFactor(Vector3<float> spread)
{
	this->mNoiseParams.spread = spread;

	ResizeNoiseBuf(mSizeZ > 1);
}


void Noise::setOctaves(int octaves)
{
	this->mNoiseParams.octaves = octaves;

	ResizeNoiseBuf(mSizeZ > 1);
}


void Noise::ResizeNoiseBuf(bool is3d)
{
	// Maximum possible spread value factor
	float ofactor = (mNoiseParams.lacunarity > 1.0) ?
		(float)pow(mNoiseParams.lacunarity, mNoiseParams.octaves - 1) : mNoiseParams.lacunarity;

	// Noise lattice point count
	// (int)(sz * spread * ofactor) is # of lattice points crossed due to length
	float numNoisePointsX = mSizeX * ofactor / mNoiseParams.spread[0];
	float numNoisePointsY = mSizeY * ofactor / mNoiseParams.spread[1];
	float numNoisePointsZ = mSizeZ * ofactor / mNoiseParams.spread[2];

	// Protect against obviously invalid parameters
	if (numNoisePointsX > 1000000000.f ||
        numNoisePointsY > 1000000000.f ||
        numNoisePointsZ > 1000000000.f)
		throw InvalidNoiseParamsException();

	// Protect against an octave having a spread < 1, causing broken noise values
	if (mNoiseParams.spread[0] / ofactor < 1.0f ||
        mNoiseParams.spread[1] / ofactor < 1.0f ||
        mNoiseParams.spread[2] / ofactor < 1.0f) 
    {
		LogWarning("A noise parameter has too many octaves: " + 
            std::to_string(mNoiseParams.octaves) + " octaves");
		throw InvalidNoiseParamsException("A noise parameter has too many octaves");
	}

	// + 2 for the two initial endpoints
	// + 1 for potentially crossing a boundary due to offset
	size_t nlx = (size_t)Function<float>::Ceil(numNoisePointsX) + 3;
	size_t nly = (size_t)Function<float>::Ceil(numNoisePointsY) + 3;
	size_t nlz = is3d ? (size_t)Function<float>::Ceil(numNoisePointsZ) + 3 : 1;

	delete[] mNoiseBuf;
	try 
    {
		mNoiseBuf = new float[nlx * nly * nlz];
	} 
    catch (std::bad_alloc&) 
    {
		throw InvalidNoiseParamsException();
	}
}


/*
 * NB:  This algorithm is not optimal in terms of space complexity.  The entire
 * integer lattice of noise points could be done as 2 lines instead, and for 3D,
 * 2 lines + 2 planes.
 * However, this would require the noise calls to be interposed with the
 * interpolation loops, which may trash the icache, leading to lower overall
 * performance.
 * Another optimization that could save half as many noise calls is to carry over
 * values from the previous noise lattice as midpoints in the new lattice for the
 * next octave.
 */
#define idx(x, y) ((y) * nlx + (x))
void Noise::GradientMap2D(float x, float y, float stepX, float stepY, int seed)
{
	float v00, v01, v10, v11, u, v, origU;
	unsigned int index, i, j, noisex, noisey;
	unsigned int nlx, nly;
	int x0, y0;

	bool eased = mNoiseParams.flags & (NOISE_FLAG_DEFAULTS | NOISE_FLAG_EASED);
	Interp2dFxn Interpolate = eased ?
		BiLinearInterpolation : BiLinearInterpolationNoEase;

	x0 = (int)Function<float>::Floor(x);
	y0 = (int)Function<float>::Floor(y);
	u = x - (float)x0;
	v = y - (float)y0;
	origU = u;

	//calculate noise point lattice
	nlx = (unsigned int)(u + mSizeX * stepX) + 2;
	nly = (unsigned int)(v + mSizeY * stepY) + 2;
	index = 0;
	for (j = 0; j != nly; j++)
		for (i = 0; i != nlx; i++)
			mNoiseBuf[index++] = Noise2d(x0 + i, y0 + j, seed);

	//calculate interpolations
	index  = 0;
	noisey = 0;
	for (j = 0; j != mSizeY; j++) 
    {
		v00 = mNoiseBuf[idx(0, noisey)];
		v10 = mNoiseBuf[idx(1, noisey)];
		v01 = mNoiseBuf[idx(0, noisey + 1)];
		v11 = mNoiseBuf[idx(1, noisey + 1)];

		u = origU;
		noisex = 0;
		for (i = 0; i != mSizeX; i++) 
        {
			mGradientBuf[index++] = Interpolate(v00, v10, v01, v11, u, v);

			u += stepX;
			if (u >= 1.0) 
            {
				u -= 1.0;
				noisex++;
				v00 = v10;
				v01 = v11;
				v10 = mNoiseBuf[idx(noisex + 1, noisey)];
				v11 = mNoiseBuf[idx(noisex + 1, noisey + 1)];
			}
		}

		v += stepY;
		if (v >= 1.0) {
			v -= 1.0;
			noisey++;
		}
	}
}
#undef idx


#define idx(x, y, z) ((z) * nly * nlx + (y) * nlx + (x))
void Noise::GradientMap3D(float x, float y, float z, float stepX, float stepY, float stepZ, int seed)
{
	float v000, v010, v100, v110;
	float v001, v011, v101, v111;
	float u, v, w, origU, origV;
	unsigned int index, i, j, k, noisex, noisey, noisez;
	unsigned int nlx, nly, nlz;
	int x0, y0, z0;

	Interp3dFxn Interpolate = (mNoiseParams.flags & NOISE_FLAG_EASED) ?
		TriLinearInterpolation : TriLinearInterpolationNoEase;

	x0 = (int)Function<float>::Floor(x);
	y0 = (int)Function<float>::Floor(y);
	z0 = (int)Function<float>::Floor(z);
	u = x - (float)x0;
	v = y - (float)y0;
	w = z - (float)z0;
	origU = u;
	origV = v;

	//calculate noise point lattice
	nlx = (unsigned int)(u + mSizeX * stepX) + 2;
	nly = (unsigned int)(v + mSizeY * stepY) + 2;
	nlz = (unsigned int)(w + mSizeZ * stepZ) + 2;
	index = 0;
	for (k = 0; k != nlz; k++)
		for (j = 0; j != nly; j++)
			for (i = 0; i != nlx; i++)
				mNoiseBuf[index++] = Noise3d(x0 + i, y0 + j, z0 + k, seed);

	//calculate interpolations
	index  = 0;
	noisey = 0;
	noisez = 0;
	for (k = 0; k != mSizeZ; k++) 
    {
		v = origV;
		noisey = 0;
		for (j = 0; j != mSizeY; j++) 
        {
			v000 = mNoiseBuf[idx(0, noisey,     noisez)];
			v100 = mNoiseBuf[idx(1, noisey,     noisez)];
			v010 = mNoiseBuf[idx(0, noisey + 1, noisez)];
			v110 = mNoiseBuf[idx(1, noisey + 1, noisez)];
			v001 = mNoiseBuf[idx(0, noisey,     noisez + 1)];
			v101 = mNoiseBuf[idx(1, noisey,     noisez + 1)];
			v011 = mNoiseBuf[idx(0, noisey + 1, noisez + 1)];
			v111 = mNoiseBuf[idx(1, noisey + 1, noisez + 1)];

			u = origU;
			noisex = 0;
			for (i = 0; i != mSizeX; i++) 
            {
				mGradientBuf[index++] = Interpolate(
					v000, v100, v010, v110,
					v001, v101, v011, v111,
					u, v, w);

				u += stepX;
				if (u >= 1.0) 
                {
					u -= 1.0;
					noisex++;
					v000 = v100;
					v010 = v110;
					v100 = mNoiseBuf[idx(noisex + 1, noisey,     noisez)];
					v110 = mNoiseBuf[idx(noisex + 1, noisey + 1, noisez)];
					v001 = v101;
					v011 = v111;
					v101 = mNoiseBuf[idx(noisex + 1, noisey,     noisez + 1)];
					v111 = mNoiseBuf[idx(noisex + 1, noisey + 1, noisez + 1)];
				}
			}

			v += stepY;
			if (v >= 1.0) 
            {
				v -= 1.0;
				noisey++;
			}
		}

		w += stepZ;
		if (w >= 1.0) 
        {
			w -= 1.0;
			noisez++;
		}
	}
}
#undef idx


float* Noise::PerlinMap2D(float x, float y, float *persistenceMap)
{
	float f = 1.0, g = 1.0;
	size_t bufsize = mSizeX * mSizeY;

	x /= mNoiseParams.spread[0];
	y /= mNoiseParams.spread[1];

	memset(mResult, 0, sizeof(float) * bufsize);

	if (persistenceMap) 
    {
		if (!mPersistBuf)
			mPersistBuf = new float[bufsize];
		for (size_t i = 0; i != bufsize; i++)
			mPersistBuf[i] = 1.0;
	}

	for (uint16_t oct = 0; oct < mNoiseParams.octaves; oct++) 
    {
		GradientMap2D(x * f, y * f,
			f / mNoiseParams.spread[0], f / mNoiseParams.spread[1],
			mSeed + mNoiseParams.seed + oct);

		UpdateResults(g, mPersistBuf, persistenceMap, bufsize);

		f *= mNoiseParams.lacunarity;
		g *= mNoiseParams.persist;
	}

	if (std::fabs(mNoiseParams.offset - 0.f) > 0.00001 || 
        std::fabs(mNoiseParams.scale - 1.f) > 0.00001)
    {
		for (size_t i = 0; i != bufsize; i++)
			mResult[i] = mResult[i] * mNoiseParams.scale + mNoiseParams.offset;
	}

	return mResult;
}


float* Noise::PerlinMap3D(float x, float y, float z, float *persistenceMap)
{
	float f = 1.0, g = 1.0;
	size_t bufsize = mSizeX * mSizeY * mSizeZ;

	x /= mNoiseParams.spread[0];
	y /= mNoiseParams.spread[1];
	z /= mNoiseParams.spread[2];

	memset(mResult, 0, sizeof(float) * bufsize);

	if (persistenceMap) 
    {
		if (!mPersistBuf)
			mPersistBuf = new float[bufsize];
		for (size_t i = 0; i != bufsize; i++)
			mPersistBuf[i] = 1.0;
	}

	for (uint16_t oct = 0; oct < mNoiseParams.octaves; oct++) 
    {
		GradientMap3D(x * f, y * f, z * f,
			f / mNoiseParams.spread[0], f / mNoiseParams.spread[1], f / mNoiseParams.spread[2],
			mSeed + mNoiseParams.seed + oct);

		UpdateResults(g, mPersistBuf, persistenceMap, bufsize);

		f *= mNoiseParams.lacunarity;
		g *= mNoiseParams.persist;
	}

	if (std::fabs(mNoiseParams.offset - 0.f) > 0.00001 || 
        std::fabs(mNoiseParams.scale - 1.f) > 0.00001)
    {
		for (size_t i = 0; i != bufsize; i++)
            mResult[i] = mResult[i] * mNoiseParams.scale + mNoiseParams.offset;
	}

	return mResult;
}


void Noise::UpdateResults(float g, float* gmap,
	const float* persistenceMap, size_t bufsize)
{
	// This looks very ugly, but it is 50-70% faster than having
	// conditional statements inside the loop
	if (mNoiseParams.flags & NOISE_FLAG_ABSVALUE) 
    {
		if (persistenceMap) 
        {
			for (size_t i = 0; i != bufsize; i++) 
            {
                mResult[i] += gmap[i] * std::fabs(mGradientBuf[i]);
				gmap[i] *= persistenceMap[i];
			}
		} 
        else
        {
			for (size_t i = 0; i != bufsize; i++)
                mResult[i] += g * std::fabs(mGradientBuf[i]);
		}
	} 
    else
    {
		if (persistenceMap) 
        {
			for (size_t i = 0; i != bufsize; i++) 
            {
                mResult[i] += gmap[i] * mGradientBuf[i];
				gmap[i] *= persistenceMap[i];
			}
		} 
        else 
        {
			for (size_t i = 0; i != bufsize; i++)
                mResult[i] += g * mGradientBuf[i];
		}
	}
}
