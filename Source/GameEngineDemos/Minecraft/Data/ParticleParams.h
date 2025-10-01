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

#ifndef PARTICLEPARAMS_H
#define PARTICLEPARAMS_H

#include "GameEngineStd.h"

#include "TileParams.h"

#include "../Games/Map/MapNode.h"

struct CommonParticleParams 
{
	bool collisionDetection = false;
	bool collisionRemoval = false;
	bool objectCollision = false;
	bool vertical = false;
	std::string texture;
	struct TileAnimationParams animation;
	uint8_t glow = 0;
	MapNode node;
    uint8_t nodeTile = 0;

	CommonParticleParams() 
    {
		animation.type = TAT_NONE;
		node.SetContent(CONTENT_IGNORE);
	}

	/* This helper is useful for copying params from
	 * ParticleSpawnerParameters to ParticleParameters */
	inline void CopyCommon(CommonParticleParams& to) const 
    {
		to.collisionDetection = collisionDetection;
		to.collisionRemoval = collisionRemoval;
		to.objectCollision = objectCollision;
		to.vertical = vertical;
		to.texture = texture;
		to.animation = animation;
		to.glow = glow;
		to.node = node;
		to.nodeTile = nodeTile;
	}
};

struct ParticleParameters : CommonParticleParams 
{
	Vector3<float> pos;
	Vector3<float> vel;
	Vector3<float> acc;
	float expTime = 1;
	float size = 1;

	void Serialize(std::ostream& os, uint16_t protocolVer) const;
	void Deserialize(std::istream& is, uint16_t protocolVer);
};

struct ParticleSpawnerParameters : CommonParticleParams 
{
	uint16_t amount = 1;
	Vector3<float> minPos, maxPos, minVel, maxVel, minAcc, maxAcc;
	float time = 1;
	float minExpTime = 1, maxExpTime = 1, minSize = 1, maxSize = 1;

	// For historical reasons no (de-)serialization methods here
};

#endif