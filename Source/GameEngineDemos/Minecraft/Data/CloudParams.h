/*
Minetest
Copyright (C) 2017 bendeutsch, Ben Deutsch <ben@bendeutsch.de>

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

#ifndef CLOUDPARAMS_H
#define CLOUDPARAMS_H

#include "GameEngineStd.h"

#include "Graphic/Resource/Color.h"

#include "Mathematic/Algebra/Vector2.h"

struct CloudParams
{
	float density;
	SColor colorBright;
	SColor colorAmbient;
	float thickness;
	float height;
	Vector2<float> speed;
};

#endif