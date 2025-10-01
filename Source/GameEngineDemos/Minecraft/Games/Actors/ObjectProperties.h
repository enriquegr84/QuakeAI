/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef OBJECTPROPERTIES_H
#define OBJECTPROPERTIES_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector2.h"

#include "Graphic/Resource/Color.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"

struct ObjectProperties
{
	unsigned short hpMax = 1;
	unsigned short breathMax = 0;
	bool physical = false;
	bool collideWithObjects = true;
	// Values are BS=1
	BoundingBox<float> collisionBox = 
        BoundingBox<float>(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
	BoundingBox<float> selectionBox = 
        BoundingBox<float>(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
	bool pointable = true;
	std::string visual = "sprite";
    std::string mesh = "";
    Vector3<float> visualSize = Vector3<float>{ 1, 1, 1 };
	std::vector<std::string> textures;
	std::string damageTextureModifier = "^[brighten";
	std::vector<SColor> colors;
    Vector2<short> spriteDiv = Vector2<short>{ 1, 1 };
	Vector2<short> initialSpriteBasePos = Vector2<short>::Zero();
	bool isVisible = true;
	bool makesFootstepSound = false;
	float stepHeight = 0.0f;
	float automaticRotate = 0.0f;
	bool automaticFaceMovementDir = false;
	float automaticFaceMovementDirOffset = 0.0f;
	bool backfaceCulling = true;
	char glow = 0;
	std::string nameTag = "";
	SColor nameTagColor = SColor(255, 255, 255, 255);
	SColor nameTagBGColor;
	float automaticFaceMovementMaxRotationPerSec = -1.0f;
	std::string infoText;
	//! For dropped items, this contains item information.
    std::string wieldItem;
	bool staticSave = true;
	float eyeHeight = 1.625f;
	float zoomFov = 0.0f;
	bool useTextureAlpha = false;
	bool shaded = true;
	bool showOnMinimap = false;

	ObjectProperties();
	std::string Dump();
	void Serialize(std::ostream& os) const;
	void Deserialize(std::istream& is);
};

#endif