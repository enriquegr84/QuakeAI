/*
Minetest
Copyright (C) 2015-2019 paramat
Copyright (C) 2015-2016 kwolekr, Ryan Kwolek

Fractal formulas from http://www.bugman123.com/Hypercomplex/index.html
by Paul Nylander, and from http://www.fractalforums.com, thank you.

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

#ifndef MAPGENERATORFRACTAL_H
#define MAPGENERATORFRACTAL_H

#include "MapGenerator.h"

///////////// MapGenerator Fractal flags
#define MGFRACTAL_TERRAIN     0x01

class BiomeManager;

extern FlagDescription FlagdescMapGeneratorFractal[];


struct MapGeneratorFractalParams : public MapGeneratorParams
{
	float caveWidth = 0.09f;
	int16_t largeCaveDepth = -33;
	uint16_t smallCaveNumMin = 0;
	uint16_t smallCaveNumMax = 0;
	uint16_t largeCaveNumMin = 0;
	uint16_t largeCaveNumMax = 2;
	float largeCaveFlooded = 0.5f;
	int16_t dungeonYmin = -31000;
	int16_t dungeonYmax = 31000;
	uint16_t fractal = 1;
	uint16_t iterations = 11;
    Vector3<float> scale = Vector3<float>{ 4096.f, 1024.f, 4096.f };
    Vector3<float> offset = Vector3<float>{ 1.52f, 0.f, 0.f };
	float sliceW = 0.0f;
	float juliaX = 0.267f;
	float juliaY = 0.2f;
	float juliaZ = 0.133f;
	float juliaW = 0.067f;

	NoiseParams noiseParamsSeabed;
	NoiseParams noiseParamsFillerDepth;
	NoiseParams noiseParamsCave1;
	NoiseParams noiseParamsCave2;
	NoiseParams noiseParamsDungeons;

	MapGeneratorFractalParams();
	~MapGeneratorFractalParams() = default;

	void ReadParams(const Settings* settings);
	void WriteParams(Settings* settings);
	void SetDefaultSettings(Settings* settings);
};


class MapGeneratorFractal : public MapGeneratorBasic
{
public:
	MapGeneratorFractal(MapGeneratorFractalParams* params, EmergeParams* emerge);
	~MapGeneratorFractal();

	virtual MapGeneratorType GetType() const { return MAPGEN_FRACTAL; }

	virtual void MakeChunk(BlockMakeData *data);
	int GetSpawnLevelAtPoint(Vector2<short> pos);
	bool GetFractalAtPoint(int16_t x, int16_t y, int16_t z);
	int16_t GenerateTerrain();

private:
	uint16_t mFormula;
	bool mJulia;
	uint16_t mFractal;
	uint16_t mIterations;
	Vector3<float> mScale;
	Vector3<float> mOffset;
	float mSliceW;
	float mJuliaX;
	float mJuliaY;
	float mJuliaZ;
	float mJuliaW;

	Noise* mNoiseSeabed = nullptr;
};

#endif