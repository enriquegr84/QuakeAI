/*
Minetest
Copyright (C) 2019 Jordach, Jordan Snelling <jordach.snelling@gmail.com>

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

#ifndef SKYPARAMS_H
#define SKYPARAMS_H

#include "GameEngineStd.h"

#include "Graphic/Resource/Color.h"

struct SkyColor
{
	SColor daySky;
	SColor dayHorizon;
	SColor dawnSky;
	SColor dawnHorizon;
	SColor nightSky;
	SColor nightHorizon;
	SColor indoors;
};

struct SkyboxParams
{
	SColor bgcolor;
	std::string type;
	std::vector<std::string> textures;
	bool clouds;
	SkyColor skyColor;
	SColor fogSunTint;
	SColor fogMoonTint;
	std::string fogTintType;
};

struct SunParams
{
	bool visible;
    std::string texture;
    std::string toneMap;
    std::string sunrise;
	bool sunriseVisible;
	float scale;
};

struct MoonParams
{
	bool visible;
    std::string texture;
    std::string toneMap;
	float scale;
};

struct StarParams
{
	bool visible;
	unsigned int count;
	SColor starcolor;
	float scale;
};

// Utility class for setting default sky, sun, moon, stars values:
class SkyboxDefaults
{
public:
	const SkyColor GetSkyColorDefaults()
	{
		SkyColor sky;
		// Horizon colors
		sky.dayHorizon = SColor(255, 144, 211, 246);
		sky.indoors = SColor(255, 100, 100, 100);
		sky.dawnHorizon = SColor(255, 186, 193, 240);
		sky.nightHorizon = SColor(255, 64, 144, 255);
		// Sky colors
		sky.daySky = SColor(255, 97, 181, 245);
		sky.dawnSky = SColor(255, 180, 186, 250);
		sky.nightSky = SColor(255, 0, 107, 255);
		return sky;
	}

	const SunParams GetSunDefaults()
	{
		SunParams sun;
		sun.visible = true;
		sun.sunriseVisible = true;
		sun.texture = "sun.png";
		sun.toneMap = "sun_tonemap.png";
		sun.sunrise = "sunrisebg.png";
		sun.scale = 1;
		return sun;
	}

	const MoonParams GetMoonDefaults()
	{
		MoonParams moon;
		moon.visible = true;
		moon.texture = "moon.png";
		moon.toneMap = "moon_tonemap.png";
		moon.scale = 1;
		return moon;
	}

	const StarParams GetStarDefaults()
	{
		StarParams stars;
		stars.visible = true;
		stars.count = 1000;
		stars.starcolor = SColor(105, 235, 235, 255);
		stars.scale = 1;
		return stars;
	}
};

#endif