/*
Minetest
Copyright (C) 2020 sfan5 <sfan5@live.de>

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

#include "ParticleParams.h"

#include "Core/Utility/Serialize.h"

#include "Core/Threading/MutexAutolock.h"

#include "Game/GameLogic.h"

#include "Application/Settings.h"

void ParticleParameters::Serialize(std::ostream& os, uint16_t protocolVer) const
{
	WriteV3Float(os, pos);
	WriteV3Float(os, vel);
	WriteV3Float(os, acc);
	WriteFloat(os, expTime);
	WriteFloat(os, size);
	WriteUInt8(os, collisionDetection);
	os << SerializeString32(texture);
	WriteUInt8(os, vertical);
	WriteUInt8(os, collisionRemoval);
	animation.Serialize(os);
	WriteUInt8(os, glow);
	WriteUInt8(os, objectCollision);
	WriteUInt16(os, node.param0);
	WriteUInt8(os, node.param2);
	WriteUInt8(os, nodeTile);
}

void ParticleParameters::Deserialize(std::istream& is, uint16_t protocolVer)
{
	pos = ReadV3Float(is);
	vel = ReadV3Float(is);
	acc = ReadV3Float(is);
	expTime = ReadFloat(is);
	size = ReadFloat(is);
	collisionDetection = ReadUInt8(is);
	texture = DeserializeString32(is);
	vertical = ReadUInt8(is);
	collisionRemoval  = ReadUInt8(is);
	animation.Deserialize(is);
	glow = ReadUInt8(is);
	objectCollision = ReadUInt8(is);
	// This is kinda awful
	uint16_t tmpParam0 = ReadUInt16(is);
	if (is.eof())
		return;
	node.param0 = tmpParam0;
	node.param2 = ReadUInt8(is);
	nodeTile = ReadUInt8(is);
}