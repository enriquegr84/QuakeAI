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

#ifndef NOISE_H
#define NOISE_H

#include "../MinecraftStd.h"

#include "Mathematic/Algebra/Vector3.h"

#include "Core/Utility/StringUtil.h"

extern FlagDescription FlagdescNoiseparams[];

#define NOISE_FLAG_DEFAULTS    0x01
#define NOISE_FLAG_EASED       0x02
#define NOISE_FLAG_ABSVALUE    0x04

//// TODO(hmmmm): implement these!
#define NOISE_FLAG_POINTBUFFER 0x08
#define NOISE_FLAG_SIMPLEX     0x10

struct NoiseParams 
{
	float offset = 0.0f;
	float scale = 1.0f;
    Vector3<float> spread = Vector3<float>{ 250, 250, 250 };
	int seed = 12345;
	uint16_t octaves = 3;
	float persist = 0.6f;
	float lacunarity = 2.0f;
	unsigned int flags = NOISE_FLAG_DEFAULTS;

	NoiseParams() = default;

	NoiseParams(float aOffset, float aScale, const Vector3<float>& aSpread, 
        int aSeed, uint16_t aOctaves, float aPersist, float aLacunarity,
		unsigned int aFlags=NOISE_FLAG_DEFAULTS)
	{
		offset     = aOffset;
		scale      = aScale;
		spread     = aSpread;
		seed       = aSeed;
		octaves    = aOctaves;
		persist    = aPersist;
		lacunarity = aLacunarity;
		flags      = aFlags;
	}
};

class Noise 
{
public:
	NoiseParams mNoiseParams;
	int mSeed;
	unsigned int mSizeX;
	unsigned int mSizeY;
	unsigned int mSizeZ;
	float* mNoiseBuf = nullptr;
	float* mGradientBuf = nullptr;
	float* mPersistBuf = nullptr;
	float* mResult = nullptr;

	Noise(const NoiseParams* np, int seed, 
        unsigned int sx, unsigned int sy, unsigned int sz=1);
	~Noise();

	void SetSize(unsigned int sx, unsigned int sy, unsigned int sz=1);
	void SetSpreadFactor(Vector3<float> spread);
	void setOctaves(int octaves);

	void GradientMap2D(float x, float y, float stepX, float stepY, int seed);
	void GradientMap3D(float x, float y, float z, float stepX, float stepY, float stepZ, int seed);

	float* PerlinMap2D(float x, float y, float* persistenceMap=NULL);
	float* PerlinMap3D(float x, float y, float z, float* persistenceMap=NULL);

	inline float* PerlinMap2DPO(float x, float xoff, float y, float yoff, 
        float* persistenceMap=NULL)
	{
		return PerlinMap2D(
			x + xoff * mNoiseParams.spread[0],
			y + yoff * mNoiseParams.spread[1],
			persistenceMap);
	}

	inline float* PerlinMap3DPO(float x, float xoff, float y, float yoff,
		float z, float zoff, float *persistenceMap=NULL)
	{
		return PerlinMap3D(
			x + xoff * mNoiseParams.spread[0],
			y + yoff * mNoiseParams.spread[1],
			z + zoff * mNoiseParams.spread[2],
			persistenceMap);
	}

private:
	void AllocBuffers();
	void ResizeNoiseBuf(bool is3d);
	void UpdateResults(float g, float* gmap, const float* persistenceMap, size_t bufsize);

};

float NoisePerlin2D(const NoiseParams* np, float x, float y, int seed);
float NoisePerlin3D(const NoiseParams* np, float x, float y, float z, int seed);

inline float NoisePerlin2DPO(NoiseParams* np, 
    float x, float xoff, float y, float yoff, int seed)
{
	return NoisePerlin2D(np,
		x + xoff * np->spread[0],
		y + yoff * np->spread[1],
		seed);
}

inline float NoisePerlin3DPO(NoiseParams* np, float x, float xoff,
	float y, float yoff, float z, float zoff, int seed)
{
	return NoisePerlin3D(np,
		x + xoff * np->spread[0],
		y + yoff * np->spread[1],
		z + zoff * np->spread[2],
		seed);
}

float EaseCurve(float t);

// Return value: -1 ... 1
float Noise2d(int x, int y, int seed);
float Noise3d(int x, int y, int z, int seed);

float Noise2dGradient(float x, float y, int seed, bool eased=true);
float Noise3dGradient(float x, float y, float z, int seed, bool eased=false);

float Noise2dPerlin(float x, float y, int seed,
		int octaves, float persistence, bool eased=true);

float Noise2dPerlinAbs(float x, float y, int seed,
		int octaves, float persistence, bool eased=true);

float Noise3dPerlin(float x, float y, float z, int seed,
		int octaves, float persistence, bool eased=false);

float Noise3dPerlinAbs(float x, float y, float z, int seed,
		int octaves, float persistence, bool eased=false);

float Contour(float v);

#endif