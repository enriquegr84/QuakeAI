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

#include "ObjectProperties.h"

#include "Core/Utility/Serialize.h"
#include <sstream>

static const SColor NullBGColor{0, 1, 1, 1};

ObjectProperties::ObjectProperties()
{
	textures.emplace_back("unknown_object.png");
	colors.emplace_back(255,255,255,255);
}

std::string ObjectProperties::Dump()
{
	std::ostringstream os(std::ios::binary);
	os << "hp_max=" << hpMax;
	os << ", breath_max=" << breathMax;
	os << ", physical=" << physical;
	os << ", collideWithObjects=" << collideWithObjects;
	os << ", collisionbox=(" << collisionBox.mMinEdge[0] << "," << 
        collisionBox.mMinEdge[1] << "," << collisionBox.mMinEdge[2] << "),(" << 
        collisionBox.mMaxEdge[0] << "," << collisionBox.mMaxEdge[1] << "," << 
        collisionBox.mMaxEdge[2] << ")";
	os << ", visual=" << visual;
	os << ", mesh=" << mesh;
	os << ", visual_size=(" << visualSize[0] << "," << visualSize[1] << "," << visualSize[2] << ")";
	os << ", textures=[";
	for (const std::string& texture : textures) 
		os << "\"" << texture << "\" ";

	os << "]";
	os << ", colors=[";
	for (const SColor& color : colors) 
    {
		os << "\"" << color.GetAlpha() << "," << color.GetRed() << ","
			<< color.GetGreen() << "," << color.GetBlue() << "\" ";
	}
	os << "]";
	os << ", spritediv=" << "(" << spriteDiv[0] << "," << spriteDiv[1] << ")";
	os << ", initial_sprite_basepos=(" << initialSpriteBasePos[0] << "," << initialSpriteBasePos[1] << ")";
	os << ", is_visible=" << isVisible;
	os << ", makes_footstep_sound=" << makesFootstepSound;
	os << ", automatic_rotate="<< automaticRotate;
	os << ", backface_culling="<< backfaceCulling;
	os << ", glow=" << glow;
	os << ", nametag=" << nameTag;
	os << ", nametag_color=" << "\"" << nameTagColor.GetAlpha() << "," << nameTagColor.GetRed()
			<< "," << nameTagColor.GetGreen() << "," << nameTagColor.GetBlue() << "\" ";

	if (nameTagBGColor != NULL)
		os << ", nametag_bgcolor=" << "\"" << nameTagColor.GetAlpha() << "," << nameTagColor.GetRed()
		   << "," << nameTagColor.GetGreen() << "," << nameTagColor.GetBlue() << "\" ";
	else
		os << ", nametag_bgcolor=null ";

    os << ", selectionbox=(" << selectionBox.mMinEdge[0] << "," <<
        selectionBox.mMinEdge[1] << "," << selectionBox.mMinEdge[2] << "),(" <<
        selectionBox.mMaxEdge[0] << "," << selectionBox.mMaxEdge[1] << "," <<
        selectionBox.mMaxEdge[2] << ")";
	os << ", pointable=" << pointable;
	os << ", static_save=" << staticSave;
	os << ", eye_height=" << eyeHeight;
	os << ", zoom_fov=" << zoomFov;
	os << ", use_texture_alpha=" << useTextureAlpha;
	os << ", damage_texture_modifier=" << damageTextureModifier;
	os << ", shaded=" << shaded;
	os << ", show_on_minimap=" << showOnMinimap;
	return os.str();
}

void ObjectProperties::Serialize(std::ostream& os) const
{
	WriteUInt8(os, 4); // PROTOCOL_VERSION >= 37
	WriteUInt16(os, hpMax);
	WriteUInt8(os, physical);
	WriteFloat(os, 0.f); // Removed property (weight)
	WriteV3Float(os, collisionBox.mMinEdge);
    WriteV3Float(os, collisionBox.mMaxEdge);
    WriteV3Float(os, selectionBox.mMinEdge);
    WriteV3Float(os, selectionBox.mMaxEdge);
	WriteUInt8(os, pointable);
	os << SerializeString16(visual);
    WriteV3Float(os, visualSize);
	WriteUInt16(os, (uint16_t)textures.size());
	for (const std::string& texture : textures)
		os << SerializeString16(texture);

	WriteV2Short(os, spriteDiv);
	WriteV2Short(os, initialSpriteBasePos);
	WriteUInt8(os, isVisible);
	WriteUInt8(os, makesFootstepSound);
	WriteFloat(os, automaticRotate);
	// Added in protocol version 14
	os << SerializeString16(mesh);
	WriteUInt16(os, (uint16_t)colors.size());
	for (SColor color : colors)
		WriteARGB8(os, color);

	WriteUInt8(os, collideWithObjects);
	WriteFloat(os, stepHeight);
	WriteUInt8(os, automaticFaceMovementDir);
	WriteFloat(os, automaticFaceMovementDirOffset);
	WriteUInt8(os, backfaceCulling);
	os << SerializeString16(nameTag);
	WriteARGB8(os, nameTagColor);
	WriteFloat(os, automaticFaceMovementMaxRotationPerSec);
	os << SerializeString16(infoText);
	os << SerializeString16(wieldItem);
	WriteInt8(os, glow);
	WriteUInt16(os, breathMax);
	WriteFloat(os, eyeHeight);
	WriteFloat(os, zoomFov);
	WriteUInt8(os, useTextureAlpha);
	os << SerializeString16(damageTextureModifier);
	WriteUInt8(os, shaded);
	WriteUInt8(os, showOnMinimap);

	if (nameTagBGColor == NULL)
		WriteARGB8(os, NullBGColor);
	else if (nameTagBGColor.GetAlpha() == 0)
		WriteARGB8(os, SColor(0, 0, 0, 0));
	else
		WriteARGB8(os, nameTagBGColor);

	// Add stuff only at the bottom.
	// Never remove anything, because we don't want new versions of this
}

void ObjectProperties::Deserialize(std::istream& is)
{
	int version = ReadUInt8(is);
	if (version != 4)
		throw SerializationError("unsupported ObjectProperties version");

	hpMax = ReadUInt16(is);
	physical = ReadUInt8(is);
	ReadUInt32(is); // removed property (weight)
	collisionBox.mMinEdge = ReadV3Float(is);
	collisionBox.mMaxEdge = ReadV3Float(is);
	selectionBox.mMinEdge = ReadV3Float(is);
	selectionBox.mMaxEdge = ReadV3Float(is);
	pointable = ReadUInt8(is);
	visual = DeserializeString16(is);
	visualSize = ReadV3Float(is);
	textures.clear();
	unsigned int textureCount = ReadUInt16(is);
	for (unsigned int i = 0; i < textureCount; i++)
		textures.push_back(DeserializeString16(is));

	spriteDiv = ReadV2Short(is);
	initialSpriteBasePos = ReadV2Short(is);
	isVisible = ReadUInt8(is);
	makesFootstepSound = ReadUInt8(is);
	automaticRotate = ReadFloat(is);
	mesh = DeserializeString16(is);
	colors.clear();
	unsigned int colorCount = ReadUInt16(is);
	for (unsigned int i = 0; i < colorCount; i++)
		colors.push_back(ReadARGB8(is));

	collideWithObjects = ReadUInt8(is);
	stepHeight = ReadFloat(is);
	automaticFaceMovementDir = ReadUInt8(is);
	automaticFaceMovementDirOffset = ReadFloat(is);
	backfaceCulling = ReadUInt8(is);
	nameTag = DeserializeString16(is);
	nameTagColor = ReadARGB8(is);
	automaticFaceMovementMaxRotationPerSec = ReadFloat(is);
	infoText = DeserializeString16(is);
	wieldItem = DeserializeString16(is);
	glow = ReadInt8(is);
	breathMax = ReadUInt16(is);
	eyeHeight = ReadFloat(is);
	zoomFov = ReadFloat(is);
	useTextureAlpha = ReadUInt8(is);
	try 
    {
		damageTextureModifier = DeserializeString16(is);
		uint8_t tmp = ReadUInt8(is);
		if (is.eof())
			return;
		shaded = tmp;
		tmp = ReadUInt8(is);
		if (is.eof())
			return;
		showOnMinimap = tmp;

		auto bgcolor = ReadARGB8(is);
		if (bgcolor != NullBGColor)
			nameTagBGColor = bgcolor;
		else
			nameTagBGColor = NULL;
	} 
    catch (SerializationError&) 
    {

    }
}
