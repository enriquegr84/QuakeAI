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

#include "Node.h"
#include "Shader.h"

#include "MeshUtil.h"

#include "../Games/Actors/Item.h"

#include "../Games/Environment/VisualEnvironment.h"

#include "Core/IO/ResourceCache.h"
#include "Core/Utility/Serialize.h"

#include "Graphic/Scene/Mesh/MeshFileLoader.h"
#include "Graphic/Scene/Mesh/AnimatedMesh.h"

#include "Application/Settings.h"

std::map<std::string, unsigned int> ContentParamTypes =
{
    {"none", CPT_NONE},
    {"light", CPT_LIGHT}
};

std::map<std::string, unsigned int> ContentParamType2s =
{
    {"none", CPT2_NONE},
    {"full", CPT2_FULL},
    {"flowingliquid", CPT2_FLOWINGLIQUID},
    {"facedir", CPT2_FACEDIR},
    {"wallmounted", CPT2_WALLMOUNTED},
    {"leveled", CPT2_LEVELED},
    {"degrotate", CPT2_DEGROTATE},
    {"meshoptions", CPT2_MESHOPTIONS},
    {"color", CPT2_COLOR},
    {"colorfacedir", CPT2_COLORED_FACEDIR},
    {"colorwallmounted", CPT2_COLORED_WALLMOUNTED},
    {"glasslikeliquidlevel", CPT2_GLASSLIKE_LIQUID_LEVEL},
    {"colordegrotate", CPT2_COLORED_DEGROTATE}
};

std::map<std::string, unsigned int> LiquidTypes =
{
    {"none", LIQUID_NONE},
    {"flowing", LIQUID_FLOWING},
    {"source", LIQUID_SOURCE}
};

std::map<std::string, unsigned int> NodeBoxTypes =
{
    {"regular", NODEBOX_REGULAR},
    {"fixed", NODEBOX_FIXED},
    {"wallmounted", NODEBOX_WALLMOUNTED},
    {"leveled", NODEBOX_LEVELED},
    {"connected", NODEBOX_CONNECTED}
};

std::map<std::string, unsigned int> NodeTypes =
{
    {"normal", NDT_NORMAL},
    {"airlike", NDT_AIRLIKE},
    {"liquid", NDT_LIQUID},
    {"flowingliquid", NDT_FLOWINGLIQUID},
    {"glasslike", NDT_GLASSLIKE},
    {"allfaces", NDT_ALLFACES},
    {"allfacesoptional", NDT_ALLFACES_OPTIONAL},
    {"torchlike", NDT_TORCHLIKE},
    {"signlike", NDT_SIGNLIKE},
    {"plantlike", NDT_PLANTLIKE},
    {"fencelike", NDT_FENCELIKE},
    {"raillike", NDT_RAILLIKE},
    {"nodebox", NDT_NODEBOX},
    {"glasslikeframed", NDT_GLASSLIKE_FRAMED},
    {"firelike", NDT_FIRELIKE},
    {"glasslikeframedoptional", NDT_GLASSLIKE_FRAMED_OPTIONAL},
    {"mesh", NDT_MESH},
    {"plantlikerooted", NDT_PLANTLIKE_ROOTED}
};

std::map<std::string, unsigned int> AlphaModes =
{
    {"blend", ALPHAMODE_BLEND},
    {"clip", ALPHAMODE_CLIP},
    {"opaque", ALPHAMODE_OPAQUE},
    {"", ALPHAMODE_LEGACY_COMPAT}
};

/*
    NodeBox
*/

void NodeBox::Reset()
{
    type = NODEBOX_REGULAR;
    // default is empty
    fixed.clear();
    // default is sign/ladder-like
    wallTop = BoundingBox<float>(-BS / 2, BS / 2 - BS / 16., -BS / 2, BS / 2, BS / 2, BS / 2);
    wallBottom = BoundingBox<float>(-BS / 2, -BS / 2, -BS / 2, BS / 2, -BS / 2 + BS / 16., BS / 2);
    wallSide = BoundingBox<float>(-BS / 2, -BS / 2, -BS / 2, -BS / 2 + BS / 16., BS / 2, BS / 2);
    // no default for other parts
    connectTop.clear();
    connectBottom.clear();
    connectFront.clear();
    connectLeft.clear();
    connectBack.clear();
    connectRight.clear();
    disconnectedTop.clear();
    disconnectedBottom.clear();
    disconnectedFront.clear();
    disconnectedLeft.clear();
    disconnectedBack.clear();
    disconnectedRight.clear();
    disconnected.clear();
    disconnectedSides.clear();
}

void NodeBox::Serialize(std::ostream& os) const
{
    // Protocol >= 36
    const unsigned char version = 6;
    WriteUInt8(os, version);

    switch (type) 
    {
        case NODEBOX_LEVELED:
        case NODEBOX_FIXED:
            WriteUInt8(os, type);

            WriteUInt16(os, (uint16_t)fixed.size());
            for (const BoundingBox<float>& nodebox : fixed) 
            {
                WriteV3Float(os, nodebox.mMinEdge);
                WriteV3Float(os, nodebox.mMaxEdge);
            }
            break;
        case NODEBOX_WALLMOUNTED:
            WriteUInt8(os, type);

            WriteV3Float(os, wallTop.mMinEdge);
            WriteV3Float(os, wallTop.mMaxEdge);
            WriteV3Float(os, wallBottom.mMinEdge);
            WriteV3Float(os, wallBottom.mMaxEdge);
            WriteV3Float(os, wallSide.mMinEdge);
            WriteV3Float(os, wallSide.mMaxEdge);
            break;
        case NODEBOX_CONNECTED:
            WriteUInt8(os, type);

    #define WRITEBOX(box) \
		    WriteUInt16(os, (uint16_t)(box).size()); \
		    for (const BoundingBox<float> &i: (box)) { \
			    WriteV3Float(os, i.mMinEdge); \
			    WriteV3Float(os, i.mMaxEdge); \
		    };

            WRITEBOX(fixed);
            WRITEBOX(connectTop);
            WRITEBOX(connectBottom);
            WRITEBOX(connectFront);
            WRITEBOX(connectLeft);
            WRITEBOX(connectBack);
            WRITEBOX(connectRight);
            WRITEBOX(disconnectedTop);
            WRITEBOX(disconnectedBottom);
            WRITEBOX(disconnectedFront);
            WRITEBOX(disconnectedLeft);
            WRITEBOX(disconnectedBack);
            WRITEBOX(disconnectedRight);
            WRITEBOX(disconnected);
            WRITEBOX(disconnectedSides);
            break;
        default:
            WriteUInt8(os, type);
            break;
    }
}

void NodeBox::Deserialize(std::istream &is)
{
    int version = ReadUInt8(is);
    if (version < 6)
        throw SerializationError("unsupported NodeBox version");

    Reset();

    type = (enum NodeBoxType)ReadUInt8(is);

    if (type == NODEBOX_FIXED || type == NODEBOX_LEVELED)
    {
        unsigned short fixed_count = ReadUInt16(is);
        while (fixed_count--)
        {
            BoundingBox<float> box;
            box.mMinEdge = ReadV3Float(is);
            box.mMaxEdge = ReadV3Float(is);
            fixed.push_back(box);
        }
    }
    else if (type == NODEBOX_WALLMOUNTED)
    {
        wallTop.mMinEdge = ReadV3Float(is);
        wallTop.mMaxEdge = ReadV3Float(is);
        wallBottom.mMinEdge = ReadV3Float(is);
        wallBottom.mMaxEdge = ReadV3Float(is);
        wallSide.mMinEdge = ReadV3Float(is);
        wallSide.mMaxEdge = ReadV3Float(is);
    }
    else if (type == NODEBOX_CONNECTED)
    {
#define READBOXES(box) { \
		count = ReadUInt16(is); \
		(box).reserve(count); \
		while (count--) { \
			Vector3<float> min = ReadV3Float(is); \
			Vector3<float> max = ReadV3Float(is); \
			(box).emplace_back(min, max); }; }

        unsigned short count;

        READBOXES(fixed);
        READBOXES(connectTop);
        READBOXES(connectBottom);
        READBOXES(connectFront);
        READBOXES(connectLeft);
        READBOXES(connectBack);
        READBOXES(connectRight);
        READBOXES(disconnectedTop);
        READBOXES(disconnectedBottom);
        READBOXES(disconnectedFront);
        READBOXES(disconnectedLeft);
        READBOXES(disconnectedBack);
        READBOXES(disconnectedRight);
        READBOXES(disconnected);
        READBOXES(disconnectedSides);
    }
}

/*
    ContentFeatures
*/

ContentFeatures::ContentFeatures()
{
    Reset();
}

ContentFeatures::~ContentFeatures()
{
    for (unsigned short j = 0; j < 6; j++) 
    {
        delete tiles[j].layers[0].frames;
        delete tiles[j].layers[1].frames;
    }
    for (unsigned short j = 0; j < CF_SPECIAL_COUNT; j++)
        delete specialTiles[j].layers[0].frames;
}

void ContentFeatures::Reset()
{
    /*
        Cached stuff
    */
    solidness = 2;
    visualSolidness = 0;
    backfaceCulling = true;

    hasOnConstruct = false;
    hasOnDestruct = false;
    hasAfterDestruct = false;
    rightClickable = false;
    /*
        Actual data

        NOTE: Most of this is always overridden by the default values given
              in builtin.script
    */
    name = "";
    groups.clear();
    // Unknown nodes can be dug
    groups["dig_immediate"] = 2;
    drawType = NDT_NORMAL;
    mesh = "";

    for (auto &i : visualMesh)
        i = NULL;
    minimapColor = SColor(0, 0, 0, 0);

    visualScale = 1.0;
    for (auto& t : tile)
        t = Tile();
    for (auto& ts : tileSpecial)
        ts = Tile();
    alpha = ALPHAMODE_OPAQUE;
    postEffectColor = SColor(0, 0, 0, 0);
    paramType = CPT_NONE;
    paramType2 = CPT2_NONE;
    isGroundContent = false;
    lightPropagates = false;
    sunlightPropagates = false;
    walkable = true;
    pointable = true;
    diggable = true;
    climbable = false;
    buildableTo = false;
    floodable = false;
    leveled = 0;
    leveledMax = LEVELED_MAX;
    liquidType = LIQUID_NONE;
    liquidAlternativeFlowing = "";
    liquidAlternativeFlowingId = CONTENT_IGNORE;
    liquidAlternativeSource = "";
    liquidAlternativeSourceId = CONTENT_IGNORE;
    liquidViscosity = 0;
    liquidRenewable = true;
    liquidRange = LIQUID_LEVEL_MAX + 1;
    drowning = 0;
    lightSource = 0;
    damagePerSecond = 0;
    nodeBox = NodeBox();
    selectionBox = NodeBox();
    collisionBox = NodeBox();
    waving = 0;
    legacyFacedirSimple = false;
    legacyWallmounted = false;
    soundFootstep = SimpleSound();
    soundDig = SimpleSound("__group");
    soundDug = SimpleSound();
    connectsTo.clear();
    connectsToIds.clear();
    connectSides = 0;
    color = SColor(0xFFFFFFFF);
    paletteName = "";
    palette = NULL;
    nodeDigPrediction = "air";
}

void ContentFeatures::SetAlphaFromLegacy(unsigned char legacyAlpha)
{
    // No special handling for nodebox/mesh here as it doesn't make sense to
    // throw warnings when the logic is too old to support the "correct" way
    switch (drawType) 
    {
        case NDT_NORMAL:
            alpha = legacyAlpha == 255 ? ALPHAMODE_OPAQUE : ALPHAMODE_CLIP;
            break;
        case NDT_LIQUID:
        case NDT_FLOWINGLIQUID:
            alpha = legacyAlpha == 255 ? ALPHAMODE_OPAQUE : ALPHAMODE_BLEND;
            break;
        default:
            alpha = legacyAlpha == 255 ? ALPHAMODE_CLIP : ALPHAMODE_BLEND;
            break;
    }
}

unsigned char ContentFeatures::GetAlphaForLegacy() const
{
    // This is so simple only because 255 and 0 mean wildly different things
    // depending on drawType...
    return alpha == ALPHAMODE_OPAQUE ? 255 : 0;
}

void ContentFeatures::Serialize(std::ostream& os) const
{
    const unsigned char version = CONTENTFEATURES_VERSION;
    WriteUInt8(os, version);

    // general
    os << SerializeString16(name);
    WriteUInt16(os, (uint16_t)groups.size());
    for (const auto& group : groups) 
    {
        os << SerializeString16(group.first);
        WriteInt16(os, group.second);
    }
    WriteUInt8(os, paramType);
    WriteUInt8(os, paramType2);

    // visual
    WriteUInt8(os, drawType);
    os << SerializeString16(mesh);
    WriteFloat(os, visualScale);
    WriteUInt8(os, 6);
    for (const Tile& td : tile)
        td.Serialize(os);
    for (const Tile& td : tileOverlay)
        td.Serialize(os);
    WriteUInt8(os, CF_SPECIAL_COUNT);
    for (const Tile& td : tileSpecial)
        td.Serialize(os);

    WriteUInt8(os, GetAlphaForLegacy());
    WriteUInt8(os, color.GetRed());
    WriteUInt8(os, color.GetGreen());
    WriteUInt8(os, color.GetBlue());
    os << SerializeString16(paletteName);
    WriteUInt8(os, waving);
    WriteUInt8(os, connectSides);
    WriteUInt16(os, (uint16_t)connectsToIds.size());
    for (unsigned short connectsToId : connectsToIds)
        WriteUInt16(os, connectsToId);
    WriteARGB8(os, postEffectColor);
    WriteUInt8(os, leveled);

    // lighting
    WriteUInt8(os, lightPropagates);
    WriteUInt8(os, sunlightPropagates);
    WriteUInt8(os, lightSource);

    // map generation
    WriteUInt8(os, isGroundContent);

    // interaction
    WriteUInt8(os, walkable);
    WriteUInt8(os, pointable);
    WriteUInt8(os, diggable);
    WriteUInt8(os, climbable);
    WriteUInt8(os, buildableTo);
    WriteUInt8(os, rightClickable);
    WriteUInt32(os, damagePerSecond);

    // liquid
    WriteUInt8(os, liquidType);
    os << SerializeString16(liquidAlternativeFlowing);
    os << SerializeString16(liquidAlternativeSource);
    WriteUInt8(os, liquidViscosity);
    WriteUInt8(os, liquidRenewable);
    WriteUInt8(os, liquidRange);
    WriteUInt8(os, drowning);
    WriteUInt8(os, floodable);

    // node boxes
    nodeBox.Serialize(os);
    selectionBox.Serialize(os);
    collisionBox.Serialize(os);

    // sound
    soundFootstep.Serialize(os);
    soundDig.Serialize(os);
    soundDug.Serialize(os);

    // legacy
    WriteUInt8(os, legacyFacedirSimple);
    WriteUInt8(os, legacyWallmounted);

    os << SerializeString16(nodeDigPrediction);
    WriteUInt8(os, leveledMax);
    WriteUInt8(os, alpha);
}

void ContentFeatures::Deserialize(std::istream& is)
{
    // version detection
    const unsigned char version = ReadUInt8(is);
    if (version < CONTENTFEATURES_VERSION)
        throw SerializationError("unsupported ContentFeatures version");

    // general
    name = DeserializeString16(is);
    groups.clear();
    unsigned int groupsSize = ReadUInt16(is);
    for (unsigned int i = 0; i < groupsSize; i++) 
    {
        std::string name = DeserializeString16(is);
        int value = ReadInt16(is);
        groups[name] = value;
    }
    paramType = (enum ContentParamType) ReadUInt8(is);
    paramType2 = (enum ContentParamType2) ReadUInt8(is);

    // visual
    drawType = (enum NodeDrawType) ReadUInt8(is);
    mesh = DeserializeString16(is);
    visualScale = ReadFloat(is);
    if (ReadUInt8(is) != 6)
        throw SerializationError("unsupported tile count");
    for (Tile& td : tile)
        td.Deserialize(is);
    for (Tile& td : tileOverlay)
        td.Deserialize(is);
    if (ReadUInt8(is) != CF_SPECIAL_COUNT)
        throw SerializationError("unsupported CF_SPECIAL_COUNT");
    for (Tile &td : tileSpecial)
        td.Deserialize(is);
    SetAlphaFromLegacy(ReadUInt8(is));
    color.SetRed(ReadUInt8(is));
    color.SetGreen(ReadUInt8(is));
    color.SetBlue(ReadUInt8(is));
    paletteName = DeserializeString16(is);
    waving = ReadUInt8(is);
    connectSides = ReadUInt8(is);
    unsigned short connectsTo_size = ReadUInt16(is);
    connectsToIds.clear();
    for (unsigned short i = 0; i < connectsTo_size; i++)
        connectsToIds.push_back(ReadUInt16(is));
    postEffectColor = ReadARGB8(is);
    leveled = ReadUInt8(is);

    // lighting-related
    lightPropagates = ReadUInt8(is);
    sunlightPropagates = ReadUInt8(is);
    lightSource = ReadUInt8(is);
    lightSource = std::min(lightSource, (unsigned char)LIGHT_MAX);

    // map generation
    isGroundContent = ReadUInt8(is);

    // interaction
    walkable = ReadUInt8(is);
    pointable = ReadUInt8(is);
    diggable = ReadUInt8(is);
    climbable = ReadUInt8(is);
    buildableTo = ReadUInt8(is);
    rightClickable = ReadUInt8(is);
    damagePerSecond = ReadUInt32(is);

    // liquid
    liquidType = (enum LiquidType) ReadUInt8(is);
    liquidAlternativeFlowing = DeserializeString16(is);
    liquidAlternativeSource = DeserializeString16(is);
    liquidViscosity = ReadUInt8(is);
    liquidRenewable = ReadUInt8(is);
    liquidRange = ReadUInt8(is);
    drowning = ReadUInt8(is);
    floodable = ReadUInt8(is);

    // node boxes
    nodeBox.Deserialize(is);
    selectionBox.Deserialize(is);
    collisionBox.Deserialize(is);

    // sounds
    soundFootstep.Deserialize(is);
    soundDig.Deserialize(is);
    soundDug.Deserialize(is);

    // read legacy properties
    legacyFacedirSimple = ReadUInt8(is);
    legacyWallmounted = ReadUInt8(is);

    try 
    {
        nodeDigPrediction = DeserializeString16(is);

        unsigned char tmp = ReadUInt8(is);
        if (is.eof()) /* ReadUInt8 doesn't throw exceptions so we have to do this */
            throw SerializationError("");
        leveledMax = tmp;

        tmp = ReadUInt8(is);
        if (is.eof())
            throw SerializationError("");
        alpha = static_cast<enum AlphaMode>(tmp);
    }
    catch (SerializationError&) 
    {

    };
}

static void FillTileAttributes(BaseTextureSource* tsrc, 
    TileLayer* layer, const TileSpec& tileSpec, const Tile& tile, SColor color,
    unsigned char materialType, unsigned int shaderId, bool backfaceCulling,
    const TextureSettings& tsettings)
{
    layer->shaderId = shaderId;
    layer->texture = tsrc->GetTextureForMesh(tile.name, &layer->textureId);
    layer->materialType = materialType;

    bool hasScale = tile.scale > 0;
    bool useAutoscale = tsettings.mAutoscaleMode == AUTOSCALE_FORCE ||
        (tsettings.mAutoscaleMode == AUTOSCALE_ENABLE && !hasScale);
    if (useAutoscale)
    {
        auto textureSize = tsrc->GetTextureOriginalSize(tsrc->GetTextureId(layer->texture));
        float baseSize = (float)tsettings.mNodeTextureSize;
        float size = (float)std::fmin(textureSize[0], textureSize[1]);
        layer->scale = (uint8_t)(std::fmax(baseSize, size) / baseSize);
    }
    else if (hasScale) 
    {
        layer->scale = tile.scale;
    }
    else 
    {
        layer->scale = 1;
    }
    if (!tileSpec.worldAligned)
        layer->scale = 1;

    layer->flagsTexture = tsrc->GetShaderFlagsTexture(layer->normalTexture ? true : false);

    // Material flags
    layer->materialFlags = 0;
    if (backfaceCulling)
        layer->materialFlags |= MATERIAL_FLAG_BACKFACE_CULLING;
    if (tile.animation.type != TAT_NONE)
        layer->materialFlags |= MATERIAL_FLAG_ANIMATION;
    if (tile.tileableHorizontal)
        layer->materialFlags |= MATERIAL_FLAG_TILEABLE_HORIZONTAL;
    if (tile.tileableVertical)
        layer->materialFlags |= MATERIAL_FLAG_TILEABLE_VERTICAL;

    // Color
    layer->hasColor = tile.hasColor;
    if (tile.hasColor)
        layer->color = tile.color;
    else
        layer->color = color;

    // Animation parameters
    int frameCount = 1;
    if (layer->materialFlags & MATERIAL_FLAG_ANIMATION) 
    {
        int frameLengthMs;
        tile.animation.DetermineParams(
            tsrc->GetTextureOriginalSize(tsrc->GetTextureId(layer->texture)),
            &frameCount, &frameLengthMs, NULL);
        layer->animationFrameCount = frameCount;
        layer->animationFrameLengthMs = frameLengthMs;
    }

    if (frameCount == 1) 
    {
        layer->materialFlags &= ~MATERIAL_FLAG_ANIMATION;
    }
    else 
    {
        std::ostringstream os(std::ios::binary);
        if (!layer->frames)
            layer->frames = new std::vector<FrameSpec>();
        layer->frames->resize(frameCount);

        for (int i = 0; i < frameCount; i++) 
        {
            FrameSpec frame;

            os.str("");
            os << tile.name;
            tile.animation.GetTextureModifer(os,
                tsrc->GetTextureOriginalSize(tsrc->GetTextureId(layer->texture)), i);

            frame.texture = tsrc->GetTextureForMesh(os.str(), &frame.textureId);
            if (layer->normalTexture)
                frame.normalTexture = tsrc->GetNormalTexture(os.str());
            frame.flagsTexture = layer->flagsTexture;
            (*layer->frames)[i] = frame;
        }
    }
}

bool ContentFeatures::TextureAlphaCheck(BaseTextureSource* tsrc, const Tile* tiles, int length)
{
    static thread_local bool longWarningPrinted = false;
    std::set<std::string> seen;

    for (int i = 0; i < length; i++) 
    {
        if (seen.find(tiles[i].name) != seen.end())
            continue;
        seen.insert(tiles[i].name);

        // Load the texture and see if there's any transparent pixels
        std::shared_ptr<Texture2> texture = tsrc->GetTexture(tiles[i].name);

        // Create the 2D texture and compute the stride and image size.
        std::shared_ptr<Texture2> image = std::make_shared<Texture2>(
            texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->HasMipmaps());

        // Copy the pixels from the decoder to the texture.
        std::memcpy(image->Get<BYTE>(), texture->GetData(), image->GetNumBytes());

        bool alphaCheck = true;
        unsigned int const numLevels = image->GetNumLevels();
        for (unsigned int level = 0; level < numLevels; ++level)
        {
            unsigned int uSize = image->GetDimensionFor(level, 0);
            unsigned int vSize = image->GetDimensionFor(level, 1);
            auto imageData = reinterpret_cast<uint32_t*>(image->GetDataFor(level));
            for (unsigned int v = 0; v < vSize; ++v)
            {
                for (unsigned int u = 0; u < uSize; ++u)
                {
                    // Ignore opaque pixels.
                    SColor imageColor;
                    imageColor.SetData(&imageData[u + uSize * v]);
                    if (imageColor.GetAlpha() < 255)
                    {
                        alphaCheck = false;
                        break;
                    }
                }
                if (!alphaCheck) break;
            }
        }

        if (alphaCheck)
            continue;
        LogWarning("Texture \"" + tiles[i].name + "\" of "
            + name + " has transparency, assuming "
            "use_texture_alpha = \"clip\".");
        if (!longWarningPrinted) 
        {
            LogWarning("  This warning can be a false-positive if "
                "unused pixels in the texture are transparent. However if "
                "it is meant to be transparent, you *MUST* update the "
                "nodemgr and set use_texture_alpha = \"clip\"! This "
                "compatibility code will be removed in a few releases.");
            longWarningPrinted = true;
        }
        return true;
    }
    return false;
}

bool IsWorldAligned(AlignStyle style, WorldAlignMode mode, NodeDrawType drawType)
{
    if (style == ALIGN_STYLE_WORLD)
        return true;
    if (mode == WORLDALIGN_DISABLE)
        return false;
    if (style == ALIGN_STYLE_USER_DEFINED)
        return true;
    if (drawType == NDT_NORMAL)
        return mode >= WORLDALIGN_FORCE;
    if (drawType == NDT_NODEBOX)
        return mode >= WORLDALIGN_FORCE_NODEBOX;
    return false;
}

void ContentFeatures::UpdateTextures(BaseTextureSource* tsrc,
    BaseShaderSource* shdsrc, const TextureSettings& tsettings)
{
    // minimap pixel color - the average color of a texture
    if (tsettings.mEnableMinimap && !tile[0].name.empty())
        minimapColor = tsrc->GetTextureAverageColor(tile[0].name);

    // Figure out the actual tiles to use
    Tile actualTile[6];
    for (unsigned int j = 0; j < 6; j++) 
    {
        actualTile[j] = tile[j];
        if (actualTile[j].name.empty())
            actualTile[j].name = "unknown_node.png";
    }
    // also the overlay tiles
    Tile actualTileOverlay[6];
    for (unsigned int j = 0; j < 6; j++)
        actualTileOverlay[j] = tileOverlay[j];
    // also the special tiles
    Tile actualTileSpecial[6];
    for (unsigned int j = 0; j < CF_SPECIAL_COUNT; j++)
        actualTileSpecial[j] = tileSpecial[j];

    bool isLiquid = false;

    if (alpha == ALPHAMODE_LEGACY_COMPAT) 
    {
        // Before working with the alpha mode, resolve any legacy kludges
        alpha = TextureAlphaCheck(tsrc, actualTile, 6) ? ALPHAMODE_CLIP : ALPHAMODE_OPAQUE;
    }

    TileMaterialType materialType = alpha == ALPHAMODE_OPAQUE ? 
        TILE_MATERIAL_OPAQUE : (alpha == ALPHAMODE_CLIP ? TILE_MATERIAL_BASIC : TILE_MATERIAL_ALPHA);

    switch (drawType) 
    {
        default:
        case NDT_NORMAL:
            solidness = 2;
            break;
        case NDT_AIRLIKE:
            solidness = 0;
            break;
        case NDT_LIQUID:
            if (tsettings.mOpaqueWater)
                alpha = ALPHAMODE_OPAQUE;
            solidness = 1;
            isLiquid = true;
            break;
        case NDT_FLOWINGLIQUID:
            solidness = 0;
            if (tsettings.mOpaqueWater)
                alpha = ALPHAMODE_OPAQUE;
            isLiquid = true;
            break;
        case NDT_GLASSLIKE:
            solidness = 0;
            visualSolidness = 1;
            break;
        case NDT_GLASSLIKE_FRAMED:
            solidness = 0;
            visualSolidness = 1;
            break;
        case NDT_GLASSLIKE_FRAMED_OPTIONAL:
            solidness = 0;
            visualSolidness = 1;
            drawType = tsettings.mConnectedGlass ? NDT_GLASSLIKE_FRAMED : NDT_GLASSLIKE;
            break;
        case NDT_ALLFACES:
            solidness = 0;
            visualSolidness = 1;
            break;
        case NDT_ALLFACES_OPTIONAL:
            if (tsettings.mLeavesStyle == LEAVES_FANCY) 
            {
                drawType = NDT_ALLFACES;
                solidness = 0;
                visualSolidness = 1;
            }
            else if (tsettings.mLeavesStyle == LEAVES_SIMPLE) 
            {
                for (unsigned int j = 0; j < 6; j++) 
                    if (!actualTileSpecial[j].name.empty())
                        actualTile[j].name = actualTileSpecial[j].name;

                drawType = NDT_GLASSLIKE;
                solidness = 0;
                visualSolidness = 1;
            }
            else 
            {
                drawType = NDT_NORMAL;
                solidness = 2;
                for (Tile& td : actualTile)
                    td.name += std::string("^[noalpha");
            }
            if (waving >= 1)
                materialType = TILE_MATERIAL_WAVING_LEAVES;
            break;
        case NDT_PLANTLIKE:
            solidness = 0;
            if (waving >= 1)
                materialType = TILE_MATERIAL_WAVING_PLANTS;
            break;
        case NDT_FIRELIKE:
            solidness = 0;
            break;
        case NDT_MESH:
        case NDT_NODEBOX:
            solidness = 0;
            if (waving == 1) 
            {
                materialType = TILE_MATERIAL_WAVING_PLANTS;
            }
            else if (waving == 2) 
            {
                materialType = TILE_MATERIAL_WAVING_LEAVES;
            }
            else if (waving == 3) 
            {
                materialType = alpha == ALPHAMODE_OPAQUE ?
                    TILE_MATERIAL_WAVING_LIQUID_OPAQUE : (alpha == ALPHAMODE_CLIP ?
                        TILE_MATERIAL_WAVING_LIQUID_BASIC : TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
            }
            break;
        case NDT_TORCHLIKE:
        case NDT_SIGNLIKE:
        case NDT_FENCELIKE:
        case NDT_RAILLIKE:
            solidness = 0;
            break;
        case NDT_PLANTLIKE_ROOTED:
            solidness = 2;
            break;
    }

    if (isLiquid) 
    {
        if (waving == 3) 
        {
            materialType = alpha == ALPHAMODE_OPAQUE ?
                TILE_MATERIAL_WAVING_LIQUID_OPAQUE : (alpha == ALPHAMODE_CLIP ?
                    TILE_MATERIAL_WAVING_LIQUID_BASIC : TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
        }
        else 
        {
            materialType = alpha == ALPHAMODE_OPAQUE ? 
                TILE_MATERIAL_LIQUID_OPAQUE : TILE_MATERIAL_LIQUID_TRANSPARENT;
        }
    }

    unsigned int tileShader = shdsrc->GetShader("Nodes", materialType, drawType);

    TileMaterialType overlayMaterial = materialType;
    if (overlayMaterial == TILE_MATERIAL_OPAQUE)
        overlayMaterial = TILE_MATERIAL_BASIC;
    else if (overlayMaterial == TILE_MATERIAL_LIQUID_OPAQUE)
        overlayMaterial = TILE_MATERIAL_LIQUID_TRANSPARENT;

    unsigned int overlayShader = shdsrc->GetShader("Nodes", overlayMaterial, drawType);

    // Tiles (fill in f->tiles[])
    for (unsigned short j = 0; j < 6; j++) 
    {
        tiles[j].worldAligned = IsWorldAligned(actualTile[j].alignStyle,
            tsettings.mWorldAlignedMode, drawType);
        FillTileAttributes(tsrc, &tiles[j].layers[0], tiles[j], actualTile[j],
            color, materialType, tileShader, actualTile[j].backfaceCulling, tsettings);
        if (!actualTileOverlay[j].name.empty())
        {
            FillTileAttributes(tsrc, &tiles[j].layers[1], tiles[j], actualTileOverlay[j],
                color, overlayMaterial, overlayShader, actualTile[j].backfaceCulling, tsettings);
        }
    }

    TileMaterialType specialMaterial = materialType;
    if (drawType == NDT_PLANTLIKE_ROOTED) 
    {
        if (waving == 1)
            specialMaterial = TILE_MATERIAL_WAVING_PLANTS;
        else if (waving == 2)
            specialMaterial = TILE_MATERIAL_WAVING_LEAVES;
    }
    unsigned int specialShader = shdsrc->GetShader("Nodes", specialMaterial, drawType);

    // Special tiles (fill in f->specialTiles[])
    for (unsigned short j = 0; j < CF_SPECIAL_COUNT; j++)
    {
        FillTileAttributes(tsrc, &specialTiles[j].layers[0], specialTiles[j], actualTileSpecial[j],
            color, specialMaterial, specialShader, actualTileSpecial[j].backfaceCulling, tsettings);
    }

    if (paramType2 == CPT2_COLOR ||
        paramType2 == CPT2_COLORED_FACEDIR ||
        paramType2 == CPT2_COLORED_WALLMOUNTED ||
        paramType2 == CPT2_COLORED_DEGROTATE)
    {
        palette = tsrc->GetPalette(paletteName);
    }

    if (drawType == NDT_MESH && !mesh.empty()) 
    {
        // Meshnode drawType
        // Read the mesh and apply scale
        std::shared_ptr<ResHandle>& resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(mesh)));
        if (resHandle)
        {
            const std::shared_ptr<MeshResourceExtraData>& extra =
                std::static_pointer_cast<MeshResourceExtraData>(resHandle->GetExtra());
            visualMesh[0] = extra->GetMesh();
        }

        if (visualMesh[0]) 
        {
            Vector3<float> scale = Vector3<float>{ 1.0, 1.0, 1.0 } * BS * visualScale;
            ScaleMesh(visualMesh[0], scale);
            RecalculateBoundingBox(visualMesh[0]);
            RecalculateNormals(visualMesh[0], true, false);
        }
    }
    
    //Cache 6dfacedir and wallmounted rotated clones of meshes
    if (tsettings.mEnableMeshCache && visualMesh[0] &&
        (paramType2 == CPT2_FACEDIR || paramType2 == CPT2_COLORED_FACEDIR)) 
    {
        for (unsigned short j = 1; j < 24; j++) 
        {
            visualMesh[j] = CloneMesh(visualMesh[0]);
            RotateMeshBy6DFaceDir(visualMesh[j], j);
            RecalculateBoundingBox(visualMesh[j]);
            RecalculateNormals(visualMesh[j], true, false);
        }
    }
    else if (tsettings.mEnableMeshCache && visualMesh[0] && 
        (paramType2 == CPT2_WALLMOUNTED || paramType2 == CPT2_COLORED_WALLMOUNTED)) 
    {
        static const unsigned char wmTo6D[6] = { 20, 0, 16 + 1, 12 + 3, 8, 4 + 2 };
        for (unsigned short j = 1; j < 6; j++) 
        {
            visualMesh[j] = CloneMesh(visualMesh[0]);
            RotateMeshBy6DFaceDir(visualMesh[j], wmTo6D[j]);
            RecalculateBoundingBox(visualMesh[j]);
            RecalculateNormals(visualMesh[j], true, false);
        }
        RotateMeshBy6DFaceDir(visualMesh[0], wmTo6D[0]);
        RecalculateBoundingBox(visualMesh[0]);
        RecalculateNormals(visualMesh[0], true, false);
    }
}

/*
    NodeManager
*/
NodeManager::NodeManager()
{
    Clear();
}


NodeManager::~NodeManager()
{
    mContentFeatures.clear();
}


void NodeManager::Clear()
{
    mContentFeatures.clear();
    mNameId.Clear();
    mNameIdWithAliases.clear();
    mGroupToItems.clear();
    mNextId = 0;
    mSelectionBoxUnion.Reset(0, 0, 0);
    mSelectionBoxIntUnion.Reset(0, 0, 0);

    ResetNodeResolveState();

    unsigned int initialLength = 0;
    initialLength = std::max(initialLength, CONTENT_UNKNOWN + (unsigned int)1);
    initialLength = std::max(initialLength, CONTENT_AIR + (unsigned int)1);
    initialLength = std::max(initialLength, CONTENT_IGNORE + (unsigned int)1);
    mContentFeatures.resize(initialLength);
    //printf("Resize %p %p", mContentFeatures.begin(), mContentFeatures.end());

    // Set CONTENT_UNKNOWN
    {
        ContentFeatures f;
        f.name = "unknown";
        // Insert directly into containers
        uint16_t c = CONTENT_UNKNOWN;
        mContentFeatures[c] = f;
        AddNameIdMapping(c, f.name);
    }

    // Set CONTENT_AIR
    {
        ContentFeatures f;
        f.name = "air";
        f.drawType = NDT_AIRLIKE;
        f.paramType = CPT_LIGHT;
        f.lightPropagates = true;
        f.sunlightPropagates = true;
        f.walkable = false;
        f.pointable = false;
        f.diggable = false;
        f.buildableTo = true;
        f.floodable = true;
        f.isGroundContent = true;
        // Insert directly into containers
        uint16_t c = CONTENT_AIR;
        mContentFeatures[c] = f;
        AddNameIdMapping(c, f.name);
    }

    // Set CONTENT_IGNORE
    {
        ContentFeatures f;
        f.name = "ignore";
        f.drawType = NDT_AIRLIKE;
        f.paramType = CPT_NONE;
        f.lightPropagates = false;
        f.sunlightPropagates = false;
        f.walkable = false;
        f.pointable = false;
        f.diggable = false;
        f.buildableTo = true; // A way to remove accidental CONTENT_IGNOREs
        f.isGroundContent = true;
        // Insert directly into containers
        uint16_t c = CONTENT_IGNORE;
        mContentFeatures[c] = f;
        AddNameIdMapping(c, f.name);
    }
}


bool NodeManager::GetId(const std::string& name, uint16_t& result) const
{
    std::unordered_map<std::string, uint16_t>::const_iterator
        i = mNameIdWithAliases.find(name);
    if (i == mNameIdWithAliases.end())
        return false;
    result = i->second;
    return true;
}


uint16_t NodeManager::GetId(const std::string& name) const
{
    uint16_t id = CONTENT_IGNORE;
    GetId(name, id);
    return id;
}


bool NodeManager::GetIds(const std::string& name, std::vector<uint16_t>& result) const
{
    //TimeTaker t("getIds", NULL, PRECISION_MICRO);
    if (name.substr(0, 6) != "group:") 
    {
        uint16_t id = CONTENT_IGNORE;
        bool exists = GetId(name, id);
        if (exists)
            result.push_back(id);
        return exists;
    }
    std::string group = name.substr(6);

    std::unordered_map<std::string, std::vector<uint16_t>>::const_iterator
        i = mGroupToItems.find(group);
    if (i == mGroupToItems.end())
        return true;

    const std::vector<uint16_t>& items = i->second;
    result.insert(result.end(), items.begin(), items.end());
    //printf("getIds: %dus\n", t.stop());
    return true;
}


const ContentFeatures& NodeManager::Get(const std::string& name) const
{
    uint16_t id = CONTENT_UNKNOWN;
    GetId(name, id);
    return Get(id);
}


// returns CONTENT_IGNORE if no free ID found
uint16_t NodeManager::AllocateId()
{
    for (uint16_t id = mNextId; id >= mNextId; ++id) 
    {
        while (id >= mContentFeatures.size())
            mContentFeatures.emplace_back();

        const ContentFeatures& f = mContentFeatures[id];
        if (f.name.empty()) 
        {
            mNextId = id + 1;
            return id;
        }
    }
    // If we arrive here, an overflow occurred in id.
    // That means no ID was found
    return CONTENT_IGNORE;
}


/*!
 * Returns the smallest box that contains all boxes
 * in the vector. Box_union is expanded.
 * @param[in]      boxes     the vector containing the boxes
 * @param[in, out] boxUnion the union of the arguments
 */
void boxVectorUnion(const std::vector<BoundingBox<float>>& boxes, BoundingBox<float>* boxUnion)
{
    for (const BoundingBox<float>& box : boxes)
        boxUnion->GrowToContain(box);
}


/*!
 * Returns a box that contains the nodebox in every case.
 * The argument node_union is expanded.
 * @param[in]      nodebox  the nodebox to be measured
 * @param[in]      features  used to decide whether the nodebox
 * can be rotated
 * @param[in, out] boxUnion the union of the arguments
 */
void GetNodeBoxUnion(const NodeBox& nodebox, const ContentFeatures& features, BoundingBox<float>* boxUnion)
{
    switch (nodebox.type) 
    {
        case NODEBOX_FIXED:
        case NODEBOX_LEVELED: 
        {
            // Raw union
            BoundingBox<float> halfProcessed(0, 0, 0, 0, 0, 0);
            boxVectorUnion(nodebox.fixed, &halfProcessed);
            // Set leveled boxes to maximal
            if (nodebox.type == NODEBOX_LEVELED)
                halfProcessed.mMaxEdge[1] = +BS / 2;

            if (features.paramType2 == CPT2_FACEDIR ||
                features.paramType2 == CPT2_COLORED_FACEDIR) 
            {
                // Get maximal coordinate
                float coords[] = {
                    fabsf(halfProcessed.mMinEdge[0]),
                    fabsf(halfProcessed.mMinEdge[1]),
                    fabsf(halfProcessed.mMinEdge[2]),
                    fabsf(halfProcessed.mMaxEdge[0]),
                    fabsf(halfProcessed.mMaxEdge[1]),
                    fabsf(halfProcessed.mMaxEdge[2]) };
                float max = 0;
                for (float coord : coords)
                    if (max < coord)
                        max = coord;

                // Add the union of all possible rotated boxes
                boxUnion->GrowToContain(-max, -max, -max);
                boxUnion->GrowToContain(+max, +max, +max);
            }
            else boxUnion->GrowToContain(halfProcessed);

            break;
        }
        case NODEBOX_WALLMOUNTED: 
        {
            // Add fix boxes
            boxUnion->GrowToContain(nodebox.wallTop);
            boxUnion->GrowToContain(nodebox.wallBottom);
            // Find maximal coordinate in the X-Z plane
            float coords[] = {
                fabsf(nodebox.wallSide.mMinEdge[0]),
                fabsf(nodebox.wallSide.mMinEdge[2]),
                fabsf(nodebox.wallSide.mMaxEdge[0]),
                fabsf(nodebox.wallSide.mMaxEdge[2]) };
            float max = 0;
            for (float coord : coords)
                if (max < coord)
                    max = coord;

            // Add the union of all possible rotated boxes
            boxUnion->GrowToContain(-max, nodebox.wallSide.mMinEdge[1], -max);
            boxUnion->GrowToContain(max, nodebox.wallSide.mMaxEdge[1], max);
            break;
        }
        case NODEBOX_CONNECTED: 
        {
            // Add all possible connected boxes
            boxVectorUnion(nodebox.fixed, boxUnion);
            boxVectorUnion(nodebox.connectTop, boxUnion);
            boxVectorUnion(nodebox.connectBottom, boxUnion);
            boxVectorUnion(nodebox.connectFront, boxUnion);
            boxVectorUnion(nodebox.connectLeft, boxUnion);
            boxVectorUnion(nodebox.connectBack, boxUnion);
            boxVectorUnion(nodebox.connectRight, boxUnion);
            boxVectorUnion(nodebox.disconnectedTop, boxUnion);
            boxVectorUnion(nodebox.disconnectedBottom, boxUnion);
            boxVectorUnion(nodebox.disconnectedFront, boxUnion);
            boxVectorUnion(nodebox.disconnectedLeft, boxUnion);
            boxVectorUnion(nodebox.disconnectedBack, boxUnion);
            boxVectorUnion(nodebox.disconnectedRight, boxUnion);
            boxVectorUnion(nodebox.disconnected, boxUnion);
            boxVectorUnion(nodebox.disconnectedSides, boxUnion);
            break;
        }
        default: 
        {
            // NODEBOX_REGULAR
            boxUnion->GrowToContain(-BS / 2, -BS / 2, -BS / 2);
            boxUnion->GrowToContain(+BS / 2, +BS / 2, +BS / 2);
        }
    }
}


inline void NodeManager::FixSelectionBoxIntUnion()
{
    mSelectionBoxIntUnion.mMinEdge[0] = (short)floorf(mSelectionBoxUnion.mMinEdge[0] / BS + 0.5f);
    mSelectionBoxIntUnion.mMinEdge[1] = (short)floorf(mSelectionBoxUnion.mMinEdge[1] / BS + 0.5f);
    mSelectionBoxIntUnion.mMinEdge[2] = (short)floorf(mSelectionBoxUnion.mMinEdge[2] / BS + 0.5f);
    mSelectionBoxIntUnion.mMaxEdge[0] = (short)ceilf(mSelectionBoxUnion.mMaxEdge[0] / BS - 0.5f);
    mSelectionBoxIntUnion.mMaxEdge[1] = (short)ceilf(mSelectionBoxUnion.mMaxEdge[1] / BS - 0.5f);
    mSelectionBoxIntUnion.mMaxEdge[2] = (short)ceilf(mSelectionBoxUnion.mMaxEdge[2] / BS - 0.5f);
}


void NodeManager::EraseIdFromGroups(uint16_t id)
{
    // For all groups in mGroupToItems...
    for (auto iterGroups = mGroupToItems.begin(); iterGroups != mGroupToItems.end();) 
    {
        // Get the group items vector.
        std::vector<uint16_t>& items = iterGroups->second;

        // Remove any occurence of the id in the group items vector.
        items.erase(std::remove(items.begin(), items.end(), id), items.end());

        // If group is empty, erase its vector from the map.
        if (items.empty())
            iterGroups = mGroupToItems.erase(iterGroups);
        else
            ++iterGroups;
    }
}


// IWritableNodeManager
uint16_t NodeManager::Set(const std::string& name, const ContentFeatures& cFeatures)
{
    // Pre-conditions
    LogAssert(name != "", "invalid");
    LogAssert(name != "ignore", "invalid");
    LogAssert(name == cFeatures.name, "invalid");

    uint16_t id = CONTENT_IGNORE;
    if (!mNameId.GetId(name, id)) 
    { // ignore aliases
        // Get new id
        id = AllocateId();
        if (id == CONTENT_IGNORE) 
        {
            LogWarning("NodeManager: Absolute limit reached");
            return CONTENT_IGNORE;
        }
        LogAssert(id != CONTENT_IGNORE, "invalid");
        AddNameIdMapping(id, name);
    }

    // If there is already ContentFeatures registered for this id, clear old groups
    if (id < mContentFeatures.size())
        EraseIdFromGroups(id);

    mContentFeatures[id] = cFeatures;
    LogInformation("NodeManager: registering content id \"" + 
        std::to_string(id) + "\": name=\"" + cFeatures.name + "\"");

    GetNodeBoxUnion(cFeatures.selectionBox, cFeatures, &mSelectionBoxUnion);
    FixSelectionBoxIntUnion();

    // Add this content to the list of all groups it belongs to
    for (const auto& group : cFeatures.groups) 
    {
        const std::string& groupName = group.first;
        mGroupToItems[ToLowerString(groupName)].push_back(id);
    }

    return id;
}


uint16_t NodeManager::AllocateDummy(const std::string& name)
{
    LogAssert(name != "", "invalid");	// Pre-condition
    ContentFeatures f;
    f.name = name;
    return Set(name, f);
}


void NodeManager::RemoveNode(const std::string& name)
{
    // Pre-condition
    LogAssert(name != "", "invalid");

    // Erase name from name ID mapping
    uint16_t id = CONTENT_IGNORE;
    if (mNameId.GetId(name, id)) 
    {
        mNameId.EraseName(name);
        mNameIdWithAliases.erase(name);
    }

    EraseIdFromGroups(id);
}


void NodeManager::UpdateAliases(BaseItemManager* itemMgr)
{
    std::set<std::string> all;
    itemMgr->GetAll(all);
    mNameIdWithAliases.clear();
    for (const std::string& name : all) 
    {
        const std::string& convertTo = itemMgr->GetAlias(name);
        uint16_t id;
        if (mNameId.GetId(convertTo, id)) 
            mNameIdWithAliases.insert(std::make_pair(name, id));
    }
}

void NodeManager::ApplyTextureOverrides(const std::vector<TextureOverride>& overrides)
{
    LogInformation("NodeManager::ApplyTextureOverrides(): Applying overrides to textures");

    for (const TextureOverride& textureOverride : overrides) 
    {
        uint16_t id;
        if (!GetId(textureOverride.id, id))
            continue; // Ignore unknown node

        ContentFeatures& cFeatures = mContentFeatures[id];

        // Override tiles
        if (textureOverride.hasTarget(OverrideTarget::TOP))
            cFeatures.tile[0].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::BOTTOM))
            cFeatures.tile[1].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::RIGHT))
            cFeatures.tile[2].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::LEFT))
            cFeatures.tile[3].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::BACK))
            cFeatures.tile[4].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::FRONT))
            cFeatures.tile[5].name = textureOverride.texture;


        // Override special tiles, if applicable
        if (textureOverride.hasTarget(OverrideTarget::SPECIAL_1))
            cFeatures.tileSpecial[0].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::SPECIAL_2))
            cFeatures.tileSpecial[1].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::SPECIAL_3))
            cFeatures.tileSpecial[2].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::SPECIAL_4))
            cFeatures.tileSpecial[3].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::SPECIAL_5))
            cFeatures.tileSpecial[4].name = textureOverride.texture;

        if (textureOverride.hasTarget(OverrideTarget::SPECIAL_6))
            cFeatures.tileSpecial[5].name = textureOverride.texture;
    }
}

void NodeManager::UpdateTextures(VisualEnvironment* env, void(*progressCallback)
    (void* progressArgs, unsigned int progress, unsigned int maxProgress), void* progressCallbackArgs)
{
    LogInformation("NodeManager::UpdateTextures(): Updating textures in node definitions");

    BaseTextureSource* tsrc = env->GetTextureSource();
    BaseShaderSource* shdsrc = env->GetShaderSource();
    TextureSettings tsettings;
    tsettings.ReadSettings();

    unsigned int size = (unsigned int)mContentFeatures.size();
    for (unsigned int i = 0; i < size; i++) 
    {
        ContentFeatures* cFeatures = &(mContentFeatures[i]);
        cFeatures->UpdateTextures(tsrc, shdsrc, tsettings);
        progressCallback(progressCallbackArgs, i, size);
    }
}

void NodeManager::Serialize(std::ostream& os) const
{
    WriteUInt8(os, 1); // version
    unsigned short count = 0;
    std::ostringstream os2(std::ios::binary);
    for (unsigned int i = 0; i < mContentFeatures.size(); i++) 
    {
        if (i == CONTENT_IGNORE || i == CONTENT_AIR || i == CONTENT_UNKNOWN)
            continue;
        const ContentFeatures* f = &mContentFeatures[i];
        if (f->name.empty())
            continue;
        WriteUInt16(os2, i);
        // Wrap it in a string to allow different lengths without
        // strict version incompatibilities
        std::ostringstream wrapperOs(std::ios::binary);
        f->Serialize(wrapperOs);
        os2 << SerializeString16(wrapperOs.str());

        // must not overflow
        unsigned short next = count + 1;
        LogAssert(next >= count, "Overflow");
        count++;
    }
    WriteUInt16(os, count);
    os << SerializeString32(os2.str());
}


void NodeManager::Deserialize(std::istream& is)
{
    Clear();
    int version = ReadUInt8(is);
    if (version != 1)
        throw SerializationError("Unsupported NodeDefinitionManager version");
    unsigned short count = ReadUInt16(is);
    std::istringstream is2(DeserializeString32(is), std::ios::binary);
    ContentFeatures f;
    for (unsigned short n = 0; n < count; n++) 
    {
        unsigned short i = ReadUInt16(is2);

        // Read it from the string wrapper
        std::istringstream wrapperIs(DeserializeString16(is2), std::ios::binary);
        f.Deserialize(wrapperIs);

        // Check error conditions
        if (i == CONTENT_IGNORE || i == CONTENT_AIR || i == CONTENT_UNKNOWN) 
        {
            LogWarning("NodeManager::Deserialize(): not changing builtin node " +
                std::to_string(i));
            continue;
        }
        if (f.name.empty()) 
        {
            LogWarning("NodeManager::Deserialize(): received empty name");
            continue;
        }

        // Ignore aliases
        unsigned short existingId;
        if (mNameId.GetId(f.name, existingId) && i != existingId) 
        {
            LogWarning("NodeManager::Deserialize(): "
                "already defined with different ID: " + f.name);
            continue;
        }

        // All is ok, add node definition with the requested ID
        if (i >= mContentFeatures.size())
            mContentFeatures.resize((unsigned int)(i)+1);
        mContentFeatures[i] = f;
        AddNameIdMapping(i, f.name);
        LogInformation("NodeDef: Deserialized " + f.name);

        GetNodeBoxUnion(f.selectionBox, f, &mSelectionBoxUnion);
        FixSelectionBoxIntUnion();
    }

    // Since liquidAlternativeFlowingId and liquidAlternativeSourceId
    // are not sent, resolve them visual-side too.
    ResolveCrossrefs();
}


void NodeManager::AddNameIdMapping(uint16_t i, const std::string& name)
{
    mNameId.Set(i, name);
    mNameIdWithAliases.emplace(name, i);
}


std::shared_ptr<NodeManager> CreateNodeManager()
{
    return std::shared_ptr<NodeManager>(new NodeManager());
}


void NodeManager::PendNodeResolve(NodeResolver* nr) const
{
    nr->mNodeManager = this;
    if (mNodeRegistrationComplete)
        nr->NodeResolveInternal();
    else
        mPendingResolveCallbacks.push_back(nr);
}


bool NodeManager::CancelNodeResolveCallback(NodeResolver* nr) const
{
    size_t len = mPendingResolveCallbacks.size();
    for (size_t i = 0; i != len; i++) 
    {
        if (nr != mPendingResolveCallbacks[i])
            continue;

        len--;
        mPendingResolveCallbacks[i] = mPendingResolveCallbacks[len];
        mPendingResolveCallbacks.resize(len);
        return true;
    }

    return false;
}


void NodeManager::RunNodeResolveCallbacks()
{
    for (size_t i = 0; i != mPendingResolveCallbacks.size(); i++) 
    {
        NodeResolver *nr = mPendingResolveCallbacks[i];
        nr->NodeResolveInternal();
    }

    mPendingResolveCallbacks.clear();
}


void NodeManager::ResetNodeResolveState()
{
    mNodeRegistrationComplete = false;
    mPendingResolveCallbacks.clear();
}

static void RemoveDupes(std::vector<uint16_t> &list)
{
    std::sort(list.begin(), list.end());
    auto newEnd = std::unique(list.begin(), list.end());
    list.erase(newEnd, list.end());
}

void NodeManager::ResolveCrossrefs()
{
    std::map<std::string, ContentFeatures*> content;
    for (ContentFeatures& cFeatures : mContentFeatures)
    {
        content[cFeatures.name] = &cFeatures;

        if (cFeatures.liquidType != LIQUID_NONE || 
            cFeatures.drawType == NDT_LIQUID || 
            cFeatures.drawType == NDT_FLOWINGLIQUID)
        {
            cFeatures.liquidAlternativeFlowingId = GetId(cFeatures.liquidAlternativeFlowing);
            cFeatures.liquidAlternativeSourceId = GetId(cFeatures.liquidAlternativeSource);
            continue;
        }
        if (cFeatures.drawType != NDT_NODEBOX || 
            cFeatures.nodeBox.type != NODEBOX_CONNECTED)
            continue;

        for (const std::string& name : cFeatures.connectsTo)
            GetIds(name, cFeatures.connectsToIds);

        RemoveDupes(cFeatures.connectsToIds);
    }
}

bool NodeManager::NodeboxConnects(MapNode from, MapNode to, unsigned char connectFace) const
{
    const ContentFeatures& f1 = Get(from);

    if ((f1.drawType != NDT_NODEBOX) || (f1.nodeBox.type != NODEBOX_CONNECTED))
        return false;

    // lookup target in connected set
    if (std::find(f1.connectsToIds.begin(), f1.connectsToIds.end(), to.param0) != f1.connectsToIds.end())
        return false;

    const ContentFeatures& f2 = Get(to);

    if ((f2.drawType == NDT_NODEBOX) && (f2.nodeBox.type == NODEBOX_CONNECTED))
    {
        // ignores actually looking if back connection exists
        return (std::find(f2.connectsToIds.begin(), f2.connectsToIds.end(), to.param0) != f2.connectsToIds.end());
    }

    // does to node declare usable faces?
    if (f2.connectSides > 0) 
    {
        if ((f2.paramType2 == CPT2_FACEDIR || f2.paramType2 == CPT2_COLORED_FACEDIR) && (connectFace >= 4)) 
        {
            static const unsigned char rot[33 * 4] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 4, 32, 16, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, // 4 - back
                8, 4, 32, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, // 8 - right
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 8, 4, 32, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, // 16 - front
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 32, 16, 8, 4 // 32 - left
            };
            return (f2.connectSides & rot[(connectFace * 4) + (to.param2 & 0x1F)]);
        }
        return (f2.connectSides & connectFace);
    }
    // the target is just a regular node, so connect no matter back connection
    return true;
}

////
//// NodeResolver
////

NodeResolver::NodeResolver()
{
    Reset();
}


NodeResolver::~NodeResolver()
{
    if (!mResolveDone && mNodeManager)
        mNodeManager->CancelNodeResolveCallback(this);
}


void NodeResolver::CloneTo(NodeResolver *res) const
{
    LogAssert(mResolveDone, 
        "NodeResolver can only be cloned after resolving has completed");
    /* We don't actually do anything significant. Since the node resolving has
     * already completed, the class that called us will already have the
     * resolved IDs in its data structures (which it copies on its own) */
    res->mNodeManager = mNodeManager;
    res->mResolveDone = true;
}


void NodeResolver::NodeResolveInternal()
{
    mNodeNamesIdx = 0;
    mNodeListSizesIdx = 0;

    ResolveNodeNames();
    mResolveDone = true;

    mNodeNames.clear();
    mNodeListSizes.clear();
}


bool NodeResolver::GetIdFromNrBacklog(uint16_t* resultOut,
    const std::string& nodeAlt, uint16_t cFallback, bool errorOnFallback)
{
    if (mNodeNamesIdx == mNodeNames.size()) 
    {
        *resultOut = cFallback;
        LogError("NodeResolver: no more nodes in list");
        return false;
    }

    uint16_t c;
    std::string name = mNodeNames[mNodeNamesIdx++];

    bool success = mNodeManager->GetId(name, c);
    if (!success && !nodeAlt.empty()) 
    {
        name = nodeAlt;
        success = mNodeManager->GetId(name, c);
    }

    if (!success) 
    {
        if (errorOnFallback)
            LogError("NodeResolver: failed to resolve node name '" + name + "'.");
        c = cFallback;
    }

    *resultOut = c;
    return success;
}


bool NodeResolver::GetIdsFromNrBacklog(std::vector<uint16_t>* resultOut,
    bool allRequired, uint16_t cFallback)
{
    bool success = true;

    if (mNodeListSizesIdx == mNodeListSizes.size()) 
    {
        LogError("NodeResolver: no more node lists");
        return false;
    }

    size_t length = mNodeListSizes[mNodeListSizesIdx++];
    while (length--) 
    {
        if (mNodeNamesIdx == mNodeNames.size()) 
        {
            LogError("NodeResolver: no more nodes in list");
            return false;
        }

        uint16_t c;
        std::string& name = mNodeNames[mNodeNamesIdx++];
        if (name.substr(0, 6) != "group:") 
        {
            if (mNodeManager->GetId(name, c)) 
            {
                resultOut->push_back(c);
            }
            else if (allRequired) 
            {
                LogError("NodeResolver: failed to resolve node name '" + name + "'.");
                resultOut->push_back(cFallback);
                success = false;
            }
        }
        else 
        {
            mNodeManager->GetIds(name, *resultOut);
        }
    }

    return success;
}

void NodeResolver::Reset(bool resolveDone)
{
    mNodeNames.clear();
    mNodeNamesIdx = 0;
    mNodeListSizes.clear();
    mNodeListSizesIdx = 0;

    mResolveDone = resolveDone;

    mNodeNames.reserve(16);
    mNodeListSizes.reserve(4);
}


/*
    NodeTimer
*/

void NodeTimer::Serialize(std::ostream& os) const
{
    LogAssert(mTimeout >= FLOAT_MIN && mTimeout <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mTimeout * FIXEDPOINT_FACTOR));

    LogAssert(mElapsed >= FLOAT_MIN && mElapsed <= FLOAT_MAX, "out of range");
    WriteInt32(os, (int)(mElapsed * FIXEDPOINT_FACTOR));
}

void NodeTimer::Deserialize(std::istream& is)
{
    mTimeout = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
    mElapsed = (float)ReadInt32(is) / FIXEDPOINT_FACTOR;
}

/*
    NodeTimerList
*/

void NodeTimerList::Serialize(std::ostream& os, uint8_t mapFormatVersion) const
{
    if (mapFormatVersion == 24) 
    {
        // Version 0 is a placeholder for "nothing to see here; go away."
        if (mTimers.empty()) 
        {
            WriteUInt8(os, 0); // version
            return;
        }
        WriteUInt8(os, 1); // version
        WriteUInt16(os, (uint16_t)mTimers.size());
    }

    if (mapFormatVersion >= 25) 
    {
        WriteUInt8(os, 2 + 4 + 4); // length of the data for a single timer
        WriteUInt16(os, (uint16_t)mTimers.size());
    }

    for (const auto& timer : mTimers) 
    {
        NodeTimer t = timer.second;
        NodeTimer nt = NodeTimer(
            t.mTimeout, t.mTimeout - (float)(timer.first - mTime), t.mPosition);
        Vector3<short> p = t.mPosition;

        uint16_t p16 = p[2] * MAP_BLOCKSIZE * MAP_BLOCKSIZE + p[1] * MAP_BLOCKSIZE + p[0];
        WriteUInt16(os, p16);
        nt.Serialize(os);
    }
}

void NodeTimerList::Deserialize(std::istream& is, uint8_t mapFormatVersion)
{
    Clear();

    if (mapFormatVersion == 24) 
    {
        uint8_t timerVersion = ReadUInt8(is);
        if (timerVersion == 0)
            return;
        if (timerVersion != 1)
            throw SerializationError("Unsupported NodeTimerList version");
    }

    if (mapFormatVersion >= 25) 
    {
        uint8_t timerDataLen = ReadUInt8(is);
        if (timerDataLen != 2 + 4 + 4)
            throw SerializationError("Unsupported NodeTimer data length");
    }

    uint16_t count = ReadUInt16(is);

    for (uint16_t i = 0; i < count; i++) 
    {
        uint16_t p16 = ReadUInt16(is);

        Vector3<short> p;
        p[2] = p16 / MAP_BLOCKSIZE / MAP_BLOCKSIZE;
        p16 &= MAP_BLOCKSIZE * MAP_BLOCKSIZE - 1;
        p[1] = p16 / MAP_BLOCKSIZE;
        p16 &= MAP_BLOCKSIZE - 1;
        p[0] = p16;

        NodeTimer t(p);
        t.Deserialize(is);
        if (t.mTimeout <= 0) 
        {
            LogWarning("NodeTimerList::Deserialize(): invalid data at position"
                "(" + std::to_string(p[0]) + "," + std::to_string(p[1]) + "," + 
                std::to_string(p[2]) + "): Ignoring.");
            continue;
        }

        if (mIterators.find(p) != mIterators.end()) 
        {
            LogWarning("NodeTimerList::Deserialize(): already set data at position"
                "(" + std::to_string(p[0]) + "," + std::to_string(p[1]) + "," +
                std::to_string(p[2]) + "): Ignoring.");
            continue;
        }

        Insert(t);
    }
}

std::vector<NodeTimer> NodeTimerList::Step(float dTime)
{
    std::vector<NodeTimer> elapsedTimers;
    mTime += dTime;
    if (mNextTriggerTime == -1. || mTime < mNextTriggerTime)
        return elapsedTimers;

    std::multimap<double, NodeTimer>::iterator itTimer = mTimers.begin();
    // Process timers
    for (; itTimer != mTimers.end() && itTimer->first <= mTime; ++itTimer)
    {
        NodeTimer timer = itTimer->second;
        timer.mElapsed = timer.mTimeout + (float)(mTime - itTimer->first);
        elapsedTimers.push_back(timer);
        mIterators.erase(timer.mPosition);
    }
    // Delete elapsed timers
    mTimers.erase(mTimers.begin(), itTimer);
    if (mTimers.empty())
        mNextTriggerTime = -1.;
    else
        mNextTriggerTime = mTimers.begin()->first;
    return elapsedTimers;
}

void TextureSettings::ReadSettings()
{
    mConnectedGlass = Settings::Get()->GetBool("connected_glass");
    mOpaqueWater = Settings::Get()->GetBool("opaque_water");
    bool smoothLighting = Settings::Get()->GetBool("smooth_lighting");
    mEnableMeshCache = Settings::Get()->GetBool("enable_mesh_cache");
    mEnableMinimap = Settings::Get()->GetBool("enable_minimap");
    mNodeTextureSize = Settings::Get()->GetUInt16("texture_min_size");
    std::string leavesStyleStr = Settings::Get()->Get("leaves_style");
    std::string worldAlignedModeStr = Settings::Get()->Get("world_aligned_mode");
    std::string autoscaleModeStr = Settings::Get()->Get("autoscale_mode");

    // Mesh cache is not supported in combination with smooth lighting
    if (smoothLighting)
        mEnableMeshCache = false;

    if (leavesStyleStr == "fancy")
        mLeavesStyle = LEAVES_FANCY;
    else if (leavesStyleStr == "simple")
        mLeavesStyle = LEAVES_SIMPLE;
    else
        mLeavesStyle = LEAVES_OPAQUE;

    if (worldAlignedModeStr == "enable")
        mWorldAlignedMode = WORLDALIGN_ENABLE;
    else if (worldAlignedModeStr == "force_solid")
        mWorldAlignedMode = WORLDALIGN_FORCE;
    else if (worldAlignedModeStr == "force_nodebox")
        mWorldAlignedMode = WORLDALIGN_FORCE_NODEBOX;
    else
        mWorldAlignedMode = WORLDALIGN_DISABLE;

    if (autoscaleModeStr == "enable")
        mAutoscaleMode = AUTOSCALE_ENABLE;
    else if (autoscaleModeStr == "force")
        mAutoscaleMode = AUTOSCALE_FORCE;
    else
        mAutoscaleMode = AUTOSCALE_DISABLE;
}
