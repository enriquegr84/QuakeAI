//========================================================================
// ItemComponent.h - ItemComponent
//
// Part of the GameCode4 Application
//
// GameCode4 is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/gamecode4/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#include "ItemComponent.h"

#include "../Games.h"
#include "../Environment/Environment.h"

#include "Graphic/Graphic.h"

#include "Application/GameApplication.h"

#include "Core/Logger/Logger.h"
#include "Game/Actor/Actor.h"

const char* ItemComponent::Name = "ItemComponent";

//---------------------------------------------------------------------------------------------------------------------
// ItemComponent
//---------------------------------------------------------------------------------------------------------------------
ItemComponent::ItemComponent(void)
{

}

bool ItemComponent::Init(tinyxml2::XMLElement* pData)
{
    mData = pData;
	return true;
}

tinyxml2::XMLElement* ItemComponent::GenerateXml(void)
{
	tinyxml2::XMLDocument doc;

	// component element
	tinyxml2::XMLElement* pComponentElement = doc.NewElement(GetName());
	return pComponentElement;
}


void ItemComponent::Update(float deltaMs)
{

}

void ItemComponent::ReadSound(tinyxml2::XMLElement* pSound, SimpleSound& sound)
{
    if (pSound)
    {
        sound.name = pSound->Attribute("name");
        sound.gain = pSound->FloatAttribute("gain", sound.gain);
        sound.fade = pSound->FloatAttribute("fade", sound.fade);
        sound.pitch = pSound->FloatAttribute("pitch", sound.pitch);
    }
}

void ItemComponent::ReadGroups(tinyxml2::XMLElement* pGroups, ItemGroupList& result)
{
    if (pGroups)
    {
        result.clear();
        pGroups = pGroups->FirstChildElement();
        for (; pGroups; pGroups = pGroups->NextSiblingElement())
        {
            std::string groupName = pGroups->Name();
            int rating = atoi(pGroups->FirstChild()->Value());

            // Insert damagegroup into toolcap
            result[groupName] = rating;
        }
    }
}

ToolCapabilities ItemComponent::ReadToolCapabilities(tinyxml2::XMLElement* pData)
{
    ToolCapabilities toolcap;

    tinyxml2::XMLElement* pFullPunchInterval = pData->FirstChildElement("FullPunchInterval");
    if (pFullPunchInterval)
        toolcap.fullPunchInterval = (float)atof(pFullPunchInterval->FirstChild()->Value());
    tinyxml2::XMLElement* pMaxDropLevel = pData->FirstChildElement("MaxDropLevel");
    if (pMaxDropLevel)
        toolcap.maxDropLevel = atoi(pMaxDropLevel->FirstChild()->Value());
    tinyxml2::XMLElement* pPunchAttackUses = pData->FirstChildElement("PunchAttackUses");
    if (pPunchAttackUses)
        toolcap.punchAttackUses = atoi(pPunchAttackUses->FirstChild()->Value());

    tinyxml2::XMLElement* pGroupCaps = pData->FirstChildElement("GroupCaps");
    if (pGroupCaps)
    {
        pGroupCaps = pGroupCaps->FirstChildElement();
        for (; pGroupCaps; pGroupCaps = pGroupCaps->NextSiblingElement())
        {
            std::string groupName = pGroupCaps->Name();

            // This will be created
            ToolGroupCap groupCap;
            // Read simple parameters
            groupCap.maxlevel = pGroupCaps->IntAttribute("maxlevel");
            groupCap.uses = pGroupCaps->IntAttribute("uses");

            // Read "times" table
            std::string times = pGroupCaps->Attribute("times");
            for (const auto& time : StringSplit(times, ','))
            {
                unsigned int ratingStart = (unsigned int)time.find_first_of('[') + 1;
                unsigned int ratingEnd = (unsigned int)time.find_first_of(']');
                int rating = atoi(time.substr(ratingStart, ratingEnd - ratingStart).c_str());
                groupCap.times[rating] = (float)atof(time.substr(time.find_first_of('=')+1).c_str());
            }

            // Insert groupcap into toolcap
            toolcap.groupCaps[groupName] = groupCap;
        }
    }

    tinyxml2::XMLElement* pDamageGroups = pData->FirstChildElement("DamageGroups");
    if (pDamageGroups)
    {
        pDamageGroups = pDamageGroups->FirstChildElement();
        for (; pDamageGroups; pDamageGroups = pDamageGroups->NextSiblingElement())
        {
            std::string groupName = pDamageGroups->Name();
            short value = atoi(pDamageGroups->FirstChild()->Value());

            // Insert damagegroup into toolcap
            toolcap.damageGroups[groupName] = value;
        }
    }
    return toolcap;
}

TileAnimationParams ItemComponent::ReadAnimation(tinyxml2::XMLElement* pData)
{
    TileAnimationParams anim;
    anim.type = TAT_NONE;
    if (!std::string(pData->Attribute("type")).empty())
        anim.type = (TileAnimationType)TileAnimationTypes[pData->Attribute("type")];

    if (anim.type == TAT_VERTICAL_FRAMES) 
    {
        // {type="vertical_frames", aspect_w=16, aspect_h=16, length=2.0}
        anim.verticalFrames.aspectWidth = pData->IntAttribute("aspectwidth", 16);
        anim.verticalFrames.aspectHeight = pData->IntAttribute("aspectheight", 16);
        anim.verticalFrames.length = pData->FloatAttribute("length", 1.f);
    }
    else if (anim.type == TAT_SHEET_2D) 
    {
        // {type="sheet_2d", frames_w=5, frames_h=3, frame_length=0.5}
        anim.sheet2D.framesWidth = pData->IntAttribute("framewidth");
        anim.sheet2D.framesHeight = pData->IntAttribute("frameheight");
        anim.sheet2D.frameLength = pData->FloatAttribute("framelength");
    }

    return anim;
}

Tile ItemComponent::ReadTile(tinyxml2::XMLElement* pData, NodeDrawType drawtype)
{
    Tile tile;

    bool defaultTiling = true;
    bool defaultCulling = true;
    switch (drawtype) 
    {
        case NDT_PLANTLIKE:
        case NDT_PLANTLIKE_ROOTED:
        case NDT_FIRELIKE:
            defaultTiling = false;
            // "break" is omitted here intentionaly, as PLANTLIKE
            // FIRELIKE drawtype both should default to having
            // backface_culling to false.
        case NDT_MESH:
        case NDT_LIQUID:
            defaultCulling = false;
            break;
        default:
            break;
    }

    // key at index -2 and value at index
    if (pData->NoChildren()) 
    {
        // "default_lava.png"
        if (pData->Attribute("name"))
            tile.name = pData->Attribute("name");
        tile.tileableVertical = defaultTiling;
        tile.tileableHorizontal = defaultTiling;
        tile.backfaceCulling = defaultCulling;
    }
    else
    {
        // name="default_lava.png"
        if (pData->Attribute("name"))
            tile.name = pData->Attribute("name");
        tinyxml2::XMLElement* pImage = pData->FirstChildElement("Image");
        if (pImage)
            tile.name = pImage->FirstChild()->Value();
        tile.tileableVertical = defaultTiling;
        tile.tileableHorizontal = defaultTiling;
        tile.backfaceCulling = defaultCulling;

        tinyxml2::XMLElement* pBackfaceCulling = pData->FirstChildElement("BackfaceCulling");
        if (pBackfaceCulling)
            tinyxml2::XMLUtil::ToBool(pBackfaceCulling->FirstChild()->Value(), &tile.backfaceCulling);

        tinyxml2::XMLElement* pTileableHorizontal = pData->FirstChildElement("TileableHorizontal");
        if (pTileableHorizontal)
            tinyxml2::XMLUtil::ToBool(pTileableHorizontal->FirstChild()->Value(), &tile.tileableHorizontal);

        tinyxml2::XMLElement* pTileableVertical = pData->FirstChildElement("TileableVertical");
        if (pTileableVertical)
            tinyxml2::XMLUtil::ToBool(pTileableVertical->FirstChild()->Value(), &tile.tileableVertical);

        tinyxml2::XMLElement* pAlignStyle = pData->FirstChildElement("AlignStyle");
        if (pAlignStyle)
        {
            if (pAlignStyle->FirstChild()->Value() == "user")
                tile.alignStyle = ALIGN_STYLE_USER_DEFINED;
            else if (pAlignStyle->FirstChild()->Value() == "world")
                tile.alignStyle = ALIGN_STYLE_WORLD;
            else
                tile.alignStyle = ALIGN_STYLE_NODE;
        }

        tile.scale = 0;
        tinyxml2::XMLElement* pScale = pData->FirstChildElement("Scale");
        if (pScale)
            tile.scale = atoi(pData->FirstChild()->Value());

        // color = ...
        tinyxml2::XMLElement* pColor = pData->FirstChildElement("Color");
        if (pColor)
        {
            int a = 0, r = 0, g = 0, b = 0;

            a = pColor->IntAttribute("a", a);
            r = pColor->IntAttribute("r", r);
            g = pColor->IntAttribute("g", g);
            b = pColor->IntAttribute("b", b);
            tile.color = SColor(a, r, g, b);

            tile.hasColor = true;
        }
        else tile.hasColor = false;

        // animation = {}
        tinyxml2::XMLElement* pAnimation = pData->FirstChildElement("Animation");
        if (pAnimation)
            tile.animation = ReadAnimation(pAnimation);
    }

    return tile;
}

void ItemComponent::ReadAABB(std::string values, BoundingBox<float>& box, float scale)
{
    values = Trim(values);
    //remove brackets
    values = values.substr(1, values.size() - 2);

    std::vector<std::string> bbValues = StringSplit(values, ',');
    box.mMinEdge[0] = (float)atof(bbValues[0].c_str()) * scale;
    box.mMinEdge[1] = (float)atof(bbValues[1].c_str()) * scale;
    box.mMinEdge[2] = (float)atof(bbValues[2].c_str()) * scale;

    box.mMaxEdge[0] = (float)atof(bbValues[3].c_str()) * scale;
    box.mMaxEdge[1] = (float)atof(bbValues[4].c_str()) * scale;
    box.mMaxEdge[2] = (float)atof(bbValues[5].c_str()) * scale;

    box.Repair();
}


void ItemComponent::ReadAABBVector(
    std::string values, std::vector<BoundingBox<float>>& boxes, float scale)
{
    values = Trim(values);
    for (auto aabbValue : StringSplit(values, '}'))
    {
        size_t index = aabbValue.find_first_of('{');
        if (index != std::string::npos)
        {
            BoundingBox<float> bbox;
            ReadAABB(aabbValue.substr(index), bbox, BS);
            boxes.push_back(bbox);
        }
    }
}

NodeBox ItemComponent::ReadNodeBox(tinyxml2::XMLElement* pData)
{
    NodeBox nodeBox;

    tinyxml2::XMLElement* pNodeBoxType = pData->FirstChildElement("Type");
    if (pNodeBoxType)
        nodeBox.type = (NodeBoxType)NodeBoxTypes[pNodeBoxType->FirstChild()->Value()];

    tinyxml2::XMLElement* pNodeBoxFixed = pData->FirstChildElement("Fixed");
    if (pNodeBoxFixed)
        ReadAABBVector(pNodeBoxFixed->FirstChild()->Value(), nodeBox.fixed, BS);
    tinyxml2::XMLElement* pNodeBoxWallTop = pData->FirstChildElement("WallTop");
    if (pNodeBoxWallTop)
        ReadAABB(pNodeBoxWallTop->FirstChild()->Value(), nodeBox.wallTop, BS);
    tinyxml2::XMLElement* pNodeBoxWallBottom = pData->FirstChildElement("WallBottom");
    if (pNodeBoxWallBottom)
        ReadAABB(pNodeBoxWallBottom->FirstChild()->Value(), nodeBox.wallBottom, BS);
    tinyxml2::XMLElement* pNodeBoxWallSide = pData->FirstChildElement("WallSide");
    if (pNodeBoxWallSide)
        ReadAABB(pNodeBoxWallSide->FirstChild()->Value(), nodeBox.wallSide, BS);

    tinyxml2::XMLElement* pNodeBoxConnectTop = pData->FirstChildElement("ConnectTop");
    if (pNodeBoxConnectTop)
        ReadAABBVector(pNodeBoxConnectTop->FirstChild()->Value(), nodeBox.connectTop, BS);
    tinyxml2::XMLElement* pNodeBoxConnectBottom = pData->FirstChildElement("ConnectBottom");
    if (pNodeBoxConnectBottom)
        ReadAABBVector(pNodeBoxConnectBottom->FirstChild()->Value(), nodeBox.connectBottom, BS);
    tinyxml2::XMLElement* pNodeBoxConnectFront = pData->FirstChildElement("ConnectFront");
    if (pNodeBoxConnectFront)
        ReadAABBVector(pNodeBoxConnectFront->FirstChild()->Value(), nodeBox.connectFront, BS);
    tinyxml2::XMLElement* pNodeBoxConnectLeft = pData->FirstChildElement("ConnectLeft");
    if (pNodeBoxConnectLeft)
        ReadAABBVector(pNodeBoxConnectLeft->FirstChild()->Value(), nodeBox.connectLeft, BS);
    tinyxml2::XMLElement* pNodeBoxConnectBack = pData->FirstChildElement("ConnectBack");
    if (pNodeBoxConnectBack)
        ReadAABBVector(pNodeBoxConnectBack->FirstChild()->Value(), nodeBox.connectBack, BS);
    tinyxml2::XMLElement* pNodeBoxConnectRight = pData->FirstChildElement("ConnectRight");
    if (pNodeBoxConnectRight)
        ReadAABBVector(pNodeBoxConnectRight->FirstChild()->Value(), nodeBox.connectRight, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedTop = pData->FirstChildElement("DisconnectedTop");
    if (pNodeBoxDisconnectedTop)
        ReadAABBVector(pNodeBoxDisconnectedTop->FirstChild()->Value(), nodeBox.disconnectedTop, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedBottom = pData->FirstChildElement("DisconnectedBottom");
    if (pNodeBoxDisconnectedBottom)
        ReadAABBVector(pNodeBoxDisconnectedBottom->FirstChild()->Value(), nodeBox.disconnectedBottom, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedFront = pData->FirstChildElement("DisconnectedFront");
    if (pNodeBoxDisconnectedFront)
        ReadAABBVector(pNodeBoxDisconnectedFront->FirstChild()->Value(), nodeBox.disconnectedFront, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedLeft = pData->FirstChildElement("DisconnectedLeft");
    if (pNodeBoxDisconnectedLeft)
        ReadAABBVector(pNodeBoxDisconnectedLeft->FirstChild()->Value(), nodeBox.disconnectedLeft, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedBack = pData->FirstChildElement("DisconnectedBack");
    if (pNodeBoxDisconnectedBack)
        ReadAABBVector(pNodeBoxDisconnectedBack->FirstChild()->Value(), nodeBox.disconnectedBack, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedRight = pData->FirstChildElement("DisconnectedRight");
    if (pNodeBoxDisconnectedRight)
        ReadAABBVector(pNodeBoxDisconnectedRight->FirstChild()->Value(), nodeBox.disconnectedRight, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnected = pData->FirstChildElement("Disconnected");
    if (pNodeBoxDisconnected)
        ReadAABBVector(pNodeBoxDisconnected->FirstChild()->Value(), nodeBox.disconnected, BS);
    tinyxml2::XMLElement* pNodeBoxDisconnectedSides = pData->FirstChildElement("DisconnectedSides");
    if (pNodeBoxDisconnectedSides)
        ReadAABBVector(pNodeBoxDisconnectedSides->FirstChild()->Value(), nodeBox.disconnectedSides, BS);

    return nodeBox;
}

void ItemComponent::ParseContentFeatures(ContentFeatures& cFeatures)
{
    /* Cache existence of some callbacks */
    if (mData->FirstChildElement("OnConstruct"))
        cFeatures.hasOnConstruct = true;
    if (mData->FirstChildElement("OnDestruct"))
        cFeatures.hasOnDestruct = true;
    if (mData->FirstChildElement("AfterDestruct"))
        cFeatures.hasAfterDestruct = true;
    if (mData->FirstChildElement("OnRightClick"))
        cFeatures.rightClickable = true;

    /* Name */
    tinyxml2::XMLNode* pNode = mData->Parent();
    tinyxml2::XMLElement* pActorElement = pNode->ToElement();
    cFeatures.name = pActorElement->Attribute("name");

    /* Drop */
    tinyxml2::XMLElement* pDrops = mData->FirstChildElement("Drops");
    if (pDrops)
    {
        pDrops = pDrops->FirstChildElement();
        for (; pDrops; pDrops = pDrops->NextSiblingElement())
        {
            const char* rarity = pDrops->Attribute("rarity");
            std::string dropName = pDrops->Attribute("name");
            std::string dropRarity = rarity ? rarity : "";
            cFeatures.drops.push_back(std::pair<std::string, std::string>(dropName, dropRarity));
        }
    }

    /* Groups */
    tinyxml2::XMLElement* pGroups = mData->FirstChildElement("Groups");
    ReadGroups(pGroups, cFeatures.groups);

    /* Visual definition */
    tinyxml2::XMLElement* pDrawType = mData->FirstChildElement("DrawType");
    if (pDrawType)
        cFeatures.drawType = (NodeDrawType)NodeTypes[pDrawType->FirstChild()->Value()];

    tinyxml2::XMLElement* pVisualScale = mData->FirstChildElement("VisualScale");
    if (pVisualScale)
        cFeatures.visualScale = (float)atof(pVisualScale->FirstChild()->Value());

    /* Meshnode model filename */
    tinyxml2::XMLElement* pMesh = mData->FirstChildElement("Mesh");
    if (pMesh)
        cFeatures.mesh = pMesh->FirstChild()->Value();

    // tiles = {}
    tinyxml2::XMLElement* pTiles = mData->FirstChildElement("Tiles");
    if (pTiles)
    {
        int i = 0;
        pTiles = pTiles->FirstChildElement();
        for (;pTiles; pTiles = pTiles->NextSiblingElement(), i++)
            cFeatures.tile[i] = ReadTile(pTiles, cFeatures.drawType);

        // Copy last value to all remaining textures
        if (i >= 1) 
        {
            Tile lastTile = cFeatures.tile[i - 1];
            while (i < 6) 
            {
                cFeatures.tile[i] = lastTile;
                i++;
            }
        }
    }

    // overlay_tiles = {}
    tinyxml2::XMLElement* pOverlayTiles = mData->FirstChildElement("OverlayTiles");
    if (pOverlayTiles)
    {
        int i = 0;
        pOverlayTiles = pOverlayTiles->FirstChildElement();
        for (; pOverlayTiles; pOverlayTiles = pOverlayTiles->NextSiblingElement(), i++)
            cFeatures.tileOverlay[i] = ReadTile(pOverlayTiles, cFeatures.drawType);

        // Copy last value to all remaining textures
        if (i >= 1)
        {
            Tile lastTile = cFeatures.tileOverlay[i - 1];
            while (i < 6)
            {
                cFeatures.tileOverlay[i] = lastTile;
                i++;
            }
        }
    }

    // special_tiles = {}
    tinyxml2::XMLElement* pSpecialTiles = mData->FirstChildElement("SpecialTiles");
    if (pSpecialTiles)
    {
        int i = 0;
        pSpecialTiles = pSpecialTiles->FirstChildElement();
        for (; pSpecialTiles; pSpecialTiles = pSpecialTiles->NextSiblingElement(), i++)
        {
            cFeatures.tileSpecial[i] = ReadTile(pSpecialTiles, cFeatures.drawType);

            if (i == CF_SPECIAL_COUNT)
                break;
        }
    }

    /* alpha & use_texture_alpha */
    // This is a bit complicated due to compatibility
    cFeatures.SetDefaultAlphaMode();

    tinyxml2::XMLElement* pUseTextureAlpha = mData->FirstChildElement("UseTextureAlpha");
    if (pUseTextureAlpha)
        cFeatures.alpha = (AlphaMode)AlphaModes[pUseTextureAlpha->FirstChild()->Value()];

    /* Other stuff */
    tinyxml2::XMLElement* pColor = mData->FirstChildElement("Color");
    if (pColor)
    {
        int a = 0, r = 0, g = 0, b = 0;

        a = pColor->IntAttribute("a", a);
        r = pColor->IntAttribute("r", r);
        g = pColor->IntAttribute("g", g);
        b = pColor->IntAttribute("b", b);
        cFeatures.color = SColor(a, r, g, b);
    }
        
    tinyxml2::XMLElement* pPalette = mData->FirstChildElement("Palette");
    if (pPalette)
        cFeatures.paletteName = pPalette->FirstChild()->Value();

    tinyxml2::XMLElement* pPostEffectColor = mData->FirstChildElement("PostEffectColor");
    if (pPostEffectColor)
    {
        int a = 0, r = 0, g = 0, b = 0;

        a = pPostEffectColor->IntAttribute("a", a);
        r = pPostEffectColor->IntAttribute("r", r);
        g = pPostEffectColor->IntAttribute("g", g);
        b = pPostEffectColor->IntAttribute("b", b);
        cFeatures.postEffectColor = SColor(a, r, g, b);
    }

    tinyxml2::XMLElement* pParamType = mData->FirstChildElement("ParamType");
    if (pParamType)
        cFeatures.paramType = (ContentParamType)ContentParamTypes[pParamType->FirstChild()->Value()];

    tinyxml2::XMLElement* pParamType2 = mData->FirstChildElement("ParamType2");
    if (pParamType2)
        cFeatures.paramType2 = (ContentParamType2)ContentParamType2s[pParamType2->FirstChild()->Value()];

    if (!cFeatures.paletteName.empty() &&
        !(cFeatures.paramType2 == CPT2_COLOR ||
        cFeatures.paramType2 == CPT2_COLORED_FACEDIR ||
        cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED ||
        cFeatures.paramType2 == CPT2_COLORED_DEGROTATE))
    {
        LogWarning("Node " + cFeatures.name + " has a palette, but not a suitable paramtype2.");
    }

    // True for all ground-like things like stone and mud, false for eg. trees
    tinyxml2::XMLElement* pIsGroundContent = mData->FirstChildElement("IsGroundContent");
    if (pIsGroundContent)
        tinyxml2::XMLUtil::ToBool(pIsGroundContent->FirstChild()->Value(), &cFeatures.isGroundContent);

    cFeatures.lightPropagates = (cFeatures.paramType == CPT_LIGHT);
    tinyxml2::XMLElement* pSunlightPropagates = mData->FirstChildElement("SunlightPropagates");
    if (pSunlightPropagates)
        tinyxml2::XMLUtil::ToBool(pSunlightPropagates->FirstChild()->Value(), &cFeatures.sunlightPropagates);

    // This is used for collision detection.
    // Also for general solidness queries.
    tinyxml2::XMLElement* pWalkable = mData->FirstChildElement("Walkable");
    if (pWalkable)
        tinyxml2::XMLUtil::ToBool(pWalkable->FirstChild()->Value(), &cFeatures.walkable);
    // Player can point to these
    tinyxml2::XMLElement* pPointable = mData->FirstChildElement("Pointable");
    if (pPointable)
        tinyxml2::XMLUtil::ToBool(pPointable->FirstChild()->Value(), &cFeatures.pointable);
    // Player can dig these
    tinyxml2::XMLElement* pDiggable = mData->FirstChildElement("Diggable");
    if (pDiggable)
        tinyxml2::XMLUtil::ToBool(pDiggable->FirstChild()->Value(), &cFeatures.diggable);
    // Player can climb these
    tinyxml2::XMLElement* pClimbable = mData->FirstChildElement("Climbable");
    if (pClimbable)
        tinyxml2::XMLUtil::ToBool(pClimbable->FirstChild()->Value(), &cFeatures.climbable);
    // Player can build on these
    tinyxml2::XMLElement* pBuildableTo = mData->FirstChildElement("BuildableTo");
    if (pBuildableTo)
        tinyxml2::XMLUtil::ToBool(pBuildableTo->FirstChild()->Value(), &cFeatures.buildableTo);
    // Liquids flow into and replace node
    tinyxml2::XMLElement* pFloodable = mData->FirstChildElement("Floodable");
    if (pFloodable)
        tinyxml2::XMLUtil::ToBool(pFloodable->FirstChild()->Value(), &cFeatures.floodable);

    // Whether the node is non-liquid, source liquid or flowing liquid
    tinyxml2::XMLElement* pLiquidType = mData->FirstChildElement("LiquidType");
    if (pLiquidType)
        cFeatures.liquidType = (LiquidType)LiquidTypes[pLiquidType->FirstChild()->Value()];

    // If the content is liquid, this is the flowing version of the liquid.
    tinyxml2::XMLElement* pLiquidAlternativeFlowing = mData->FirstChildElement("LiquidAlternativeFlowing");
    if (pLiquidAlternativeFlowing)
        cFeatures.liquidAlternativeFlowing = pLiquidAlternativeFlowing->FirstChild()->Value();
    // If the content is liquid, this is the source version of the liquid.
    tinyxml2::XMLElement* pLiquidAlternativeSource = mData->FirstChildElement("LiquidAlternativeSource");
    if (pLiquidAlternativeSource)
        cFeatures.liquidAlternativeSource = pLiquidAlternativeSource->FirstChild()->Value();
    // Viscosity for fluid flow, ranging from 1 to 7, with
    // 1 giving almost instantaneous propagation and 7 being
    // the slowest possible
    tinyxml2::XMLElement* pLiquidViscosity = mData->FirstChildElement("LiquidViscosity");
    if (pLiquidViscosity)
        cFeatures.liquidViscosity = atoi(pLiquidViscosity->FirstChild()->Value());
    tinyxml2::XMLElement* pLiquidRange = mData->FirstChildElement("LiquidRange");
    if (pLiquidRange)
        cFeatures.liquidRange = atoi(pLiquidRange->FirstChild()->Value());
    tinyxml2::XMLElement* pLeveled = mData->FirstChildElement("Leveled");
    if (pLeveled)
        cFeatures.leveled = atoi(pLeveled->FirstChild()->Value());
    tinyxml2::XMLElement* pLeveledMax = mData->FirstChildElement("LeveledMax");
    if (pLeveledMax)
        cFeatures.leveledMax = atoi(pLeveledMax->FirstChild()->Value());

    tinyxml2::XMLElement* pLiquidRenewable = mData->FirstChildElement("LiquidRenewable");
    if (pLiquidRenewable)
        tinyxml2::XMLUtil::ToBool(pLiquidRenewable->FirstChild()->Value(), &cFeatures.liquidRenewable);
    tinyxml2::XMLElement* pDrowning = mData->FirstChildElement("Drowning");
    if (pDrowning)
        cFeatures.drowning = atoi(pDrowning->FirstChild()->Value());

    // Amount of light the node emits
    tinyxml2::XMLElement* pLightSource = mData->FirstChildElement("LightSource");
    if (pLightSource)
        cFeatures.lightSource = atoi(pLightSource->FirstChild()->Value());
    if (cFeatures.lightSource > LIGHT_MAX) 
    {
        LogWarning("Node " + cFeatures.name + " had greater light_source than " + 
            std::to_string(LIGHT_MAX) + ", it was reduced.");
        cFeatures.lightSource = LIGHT_MAX;
    }
    tinyxml2::XMLElement* pDamagePerSecond = mData->FirstChildElement("DamagePerSecond");
    if (pDamagePerSecond)
        cFeatures.damagePerSecond = atoi(pDamagePerSecond->FirstChild()->Value());

    tinyxml2::XMLElement* pNodeBox = mData->FirstChildElement("NodeBox");
    if (pNodeBox)
        cFeatures.nodeBox = ReadNodeBox(pNodeBox);
    tinyxml2::XMLElement* pSelectionBox = mData->FirstChildElement("SelectionBox");
    if (pSelectionBox)
        cFeatures.selectionBox = ReadNodeBox(pSelectionBox);
    tinyxml2::XMLElement* pCollisionBox = mData->FirstChildElement("CollisionBox");
    if (pCollisionBox)
        cFeatures.collisionBox = ReadNodeBox(pCollisionBox);

    tinyxml2::XMLElement* pWaving = mData->FirstChildElement("Waving");
    if (pWaving)
        cFeatures.waving = atoi(pWaving->FirstChild()->Value());

    // Set to true if paramtype used to be 'FacedirSimple'
    tinyxml2::XMLElement* pLegacyFacedirSimple = mData->FirstChildElement("LegacyFacedirSimple");
    if (pLegacyFacedirSimple)
        tinyxml2::XMLUtil::ToBool(pLegacyFacedirSimple->FirstChild()->Value(), &cFeatures.legacyFacedirSimple);
    // Set to true if wallmounted used to be set to true
    tinyxml2::XMLElement* pLegacyWallmounted = mData->FirstChildElement("LegacyWallmounted");
    if (pLegacyWallmounted)
        tinyxml2::XMLUtil::ToBool(pLegacyWallmounted->FirstChild()->Value(), &cFeatures.legacyWallmounted);

    // Sound table
    tinyxml2::XMLElement* pSound = mData->FirstChildElement("Sounds");
    if (pSound)
    {
        ReadSound(pSound->FirstChildElement("Footstep"), cFeatures.soundFootstep);
        ReadSound(pSound->FirstChildElement("Place"), cFeatures.soundPlace);
        ReadSound(pSound->FirstChildElement("Dig"), cFeatures.soundDig);
        ReadSound(pSound->FirstChildElement("Dug"), cFeatures.soundDug);
    }

    // Node immediately placed by visual when node is dug
    tinyxml2::XMLElement* pNodeDigPrediction = mData->FirstChildElement("NodeDigPrediction");
    if (pNodeDigPrediction)
        cFeatures.nodeDigPrediction = pNodeDigPrediction->FirstChild()->Value();
}

void ItemComponent::ReadItem(tinyxml2::XMLElement* pData, Item& item)
{
    tinyxml2::XMLNode* pNode = pData->Parent();
    tinyxml2::XMLElement* pActorElement = pNode->ToElement();
    item.name = pActorElement->Attribute("name");
    item.description = pActorElement->Attribute("description");

    if (ItemTypes.find(pActorElement->Attribute("type")) != ItemTypes.end())
        item.type = (ItemType)ItemTypes[pActorElement->Attribute("type")];

    tinyxml2::XMLElement* pInventoryImage = pData->FirstChildElement("InventoryImage");
    if (pInventoryImage)
        item.inventoryImage = pInventoryImage->FirstChild()->Value();
    tinyxml2::XMLElement* pInventoryOverlay = pData->FirstChildElement("InventoryOverlay");
    if (pInventoryOverlay)
        item.inventoryOverlay = pInventoryOverlay->FirstChild()->Value();
    tinyxml2::XMLElement* pWieldImage = pData->FirstChildElement("WieldImage");
    if (pWieldImage)
        item.wieldImage = pWieldImage->FirstChild()->Value();
    tinyxml2::XMLElement* pWieldOverlay = pData->FirstChildElement("WieldOverlay");
    if (pWieldOverlay)
        item.wieldOverlay = pWieldOverlay->FirstChild()->Value();
    tinyxml2::XMLElement* pPalette = pData->FirstChildElement("Palette");
    if (pPalette)
        item.paletteImage = pPalette->FirstChild()->Value();

    // Read item color.
    tinyxml2::XMLElement* pColor = pData->FirstChildElement("Color");
    if (pColor)
    {
        int a = 0, r = 0, g = 0, b = 0;

        a = pColor->IntAttribute("a", a);
        r = pColor->IntAttribute("r", r);
        g = pColor->IntAttribute("g", g);
        b = pColor->IntAttribute("b", b);
        item.color = SColor(a, r, g, b);
    }

    tinyxml2::XMLElement* pWieldScale = pData->FirstChildElement("WieldScale");
    if (pWieldScale)
    {
        float x = 0, y = 0, z = 0;

        x = pWieldScale->FloatAttribute("x", x);
        y = pWieldScale->FloatAttribute("y", y);
        z = pWieldScale->FloatAttribute("z", z);
        item.wieldScale = Vector3<float>{x, y, z};
    }

    tinyxml2::XMLElement* pStackMax = pData->FirstChildElement("StackMax");
    if (pStackMax)
        item.stackMax = std::clamp(atoi(pStackMax->FirstChild()->Value()), 1, 0xFFFF);
    tinyxml2::XMLElement* pOnUse = pData->FirstChildElement("OnUse");
    if (pOnUse)
        item.usable = atoi(pOnUse->FirstChild()->Value());
    tinyxml2::XMLElement* pLiquidsPointable = pData->FirstChildElement("LiquidsPointable");
    if (pLiquidsPointable)
        item.liquidsPointable = atoi(pLiquidsPointable->FirstChild()->Value());

    tinyxml2::XMLElement* pToolCapabilities = pData->FirstChildElement("ToolCapabilities");
    if (pToolCapabilities)
        item.toolCapabilities = new ToolCapabilities(ReadToolCapabilities(pToolCapabilities));

    // If name is "" (hand), ensure there are ToolCapabilities
    // because it will be looked up there whenever any other item has
    // no ToolCapabilities
    if (item.name.empty() && item.toolCapabilities == nullptr)
        item.toolCapabilities = new ToolCapabilities();

    tinyxml2::XMLElement* pGroups = pData->FirstChildElement("Groups");
    ReadGroups(pGroups, item.groups);

    tinyxml2::XMLElement* pSound = pData->FirstChildElement("Sounds");
    if (pSound)
    {
        pSound = pSound->FirstChildElement("Place");
        ReadSound(pSound, item.soundPlace);
        pSound = pSound->FirstChildElement("PlaceFailed");
        ReadSound(pSound, item.soundPlaceFailed);
    }

    tinyxml2::XMLElement* pRange = pData->FirstChildElement("Range");
    if (pRange)
        item.range = (float)atof(pRange->FirstChild()->Value());

    // Visual shall immediately place this node when player places the item.
    // Logic will update the precise end result a moment later.
    // "" = no prediction
    tinyxml2::XMLElement* pNodePlacementPrediction = pData->FirstChildElement("NodePlacementPrediction");
    if (pNodePlacementPrediction)
        item.nodePlacementPrediction = pNodePlacementPrediction->FirstChild()->Value();

    tinyxml2::XMLElement* pPlaceParam2 = pData->FirstChildElement("PlaceParam2");
    if (pPlaceParam2)
        item.placeParam2 = atoi(pPlaceParam2->FirstChild()->Value());
}

int ItemComponent::RegisterItem(Environment* env)
{
    NodeManager* nodeMgr = env->GetNodeManager();
    BaseWritableItemManager* itemMgr =
        static_cast<BaseWritableItemManager*>(env->GetItemManager());

    // Check if on_use is defined
    tinyxml2::XMLNode* pNode = mData->Parent();
    tinyxml2::XMLElement* pActorElement = pNode->ToElement();

    if (ItemTypes.find(pActorElement->Attribute("type")) != ItemTypes.end())
    {
        // Apply defaults and add to registered_* table
        if (ItemTypes[pActorElement->Attribute("type")] == ITEM_NODE)
        {
            tinyxml2::XMLElement* pDrawType = mData->FirstChildElement("DrawType");
            tinyxml2::XMLElement* pSelectionBox = mData->FirstChildElement("SelectionBox");
            if (pDrawType)
            {
                // Use the nodebox as selection box if it's not set manually
                if (pDrawType->FirstChild()->Value() == std::string("nodebox") && !pSelectionBox)
                {
                    pSelectionBox = mData->GetDocument()->NewElement("SelectionBox");

                    tinyxml2::XMLNode* pNodeBox = mData->FirstChildElement("NodeBox")->FirstChildElement();
                    for (; pNodeBox; pNodeBox = pNodeBox->NextSiblingElement())
                        pSelectionBox->LinkEndChild(pNodeBox->ShallowClone(pSelectionBox->GetDocument()));

                    mData->InsertEndChild(pSelectionBox);
                }
                else if (pDrawType->FirstChild()->Value() == std::string("fencelike") && !pSelectionBox)
                {
                    pSelectionBox = mData->GetDocument()->NewElement("SelectionBox");

                    tinyxml2::XMLElement* pSelectionBoxType = mData->GetDocument()->NewElement("Type");
                    pSelectionBoxType->LinkEndChild(mData->GetDocument()->NewText("fixed"));
                    pSelectionBox->LinkEndChild(pSelectionBoxType);

                    tinyxml2::XMLElement* pSelectionBoxFixed = mData->GetDocument()->NewElement("Fixed");
                    pSelectionBoxFixed->LinkEndChild(mData->GetDocument()->NewText(
                        "{-0.125, -0.5, -0.125, 0.125, 0.5, 0.125}"));
                    pSelectionBox->LinkEndChild(pSelectionBoxFixed);

                    mData->InsertEndChild(pSelectionBox);
                }
            }

            tinyxml2::XMLElement* pLightSource = mData->FirstChildElement("LightSource");
            if (pLightSource && atoi(pLightSource->FirstChild()->Value()) > LIGHT_MAX)
            {
                pLightSource->FirstChild()->SetValue(std::to_string(LIGHT_MAX).c_str());
                LogWarning("Node LightSource value exceeds maximum, limiting to maximum: ");
            }

            // Flowing liquid uses param2
            tinyxml2::XMLElement* pLiquidType = mData->FirstChildElement("LiquidType");
            if (pLiquidType && pLiquidType->FirstChild()->Value() == std::string("flowing"))
            {
                tinyxml2::XMLElement* pParamType2 = mData->FirstChildElement("ParamType2");
                if (!pParamType2)
                {
                    pParamType2 = mData->GetDocument()->NewElement("ParamType2");
                    pParamType2->LinkEndChild(mData->GetDocument()->NewText("flowingliquid"));

                    mData->InsertEndChild(pParamType2);
                }
                else pParamType2->FirstChild()->SetValue("flowingliquid");
            }
        }
        else if (ItemTypes[pActorElement->Attribute("type")] == ITEM_CRAFT)
        {
            tinyxml2::XMLElement* pImage = mData->FirstChildElement("Image");
            tinyxml2::XMLElement* pInventoryImage = mData->FirstChildElement("InventoryImage");
            if (pImage && !pInventoryImage)
            {
                pInventoryImage = (tinyxml2::XMLElement*)pImage->ShallowClone(mData->GetDocument());

                mData->InsertEndChild(pInventoryImage);
            }
        }
        else if (ItemTypes[pActorElement->Attribute("type")] == ITEM_TOOL)
        {
            tinyxml2::XMLElement* pStackMax = mData->GetDocument()->NewElement("StackMax");
            pStackMax->LinkEndChild(mData->GetDocument()->NewText("1"));
            mData->InsertFirstChild(pStackMax);

            tinyxml2::XMLElement* pImage = mData->FirstChildElement("Image");
            tinyxml2::XMLElement* pInventoryImage = mData->FirstChildElement("InventoryImage");
            if (pImage && !pInventoryImage)
            {
                pInventoryImage = (tinyxml2::XMLElement*)pImage->ShallowClone(mData->GetDocument());

                mData->InsertEndChild(pInventoryImage);
            }
        }
        else if (ItemTypes[pActorElement->Attribute("type")] == ITEM_NONE)
        {

        }
    }
    else
    {
        LogError("Unable to register item: Type is invalid");
        return 0;
    }

    // Check name
    if (!std::string(pActorElement->Attribute("name")).empty())
    {
        if (itemMgr->IsKnown(pActorElement->Attribute("name")))
        {
            LogWarning("Item already registered");
            return 0;
        }
    }

    LogInformation("Registering item: " + std::string(pActorElement->Attribute("name")));

    Item item;
    if (pActorElement->Attribute("resource"))
        item = itemMgr->Get(pActorElement->Attribute("resource"));

    // Set a distinctive default value to check if this is set
    item.nodePlacementPrediction = "__default";

    // Read the item definition
    ReadItem(mData, item);

    // Default to having visual-side placement prediction for nodes
    // ("" in item definition sets it off)
    if (item.nodePlacementPrediction == "__default")
    {
        if (item.type == ITEM_NODE)
            item.nodePlacementPrediction = item.name;
        else
            item.nodePlacementPrediction = "";
    }

    // Register item definition
    itemMgr->RegisterItem(item);
    BaseGame::Get()->RegisterItem(mData);

    // Read the node definition (content features) and register it
    if (item.type == ITEM_NODE)
    {
        ContentFeatures cFeatures;
        if (pActorElement->Attribute("resource"))
            cFeatures = nodeMgr->Get(pActorElement->Attribute("resource"));

        ParseContentFeatures(cFeatures);
        // when a mod reregisters ignore, only texture changes and such should
        // be done
        if (cFeatures.name == "ignore")
            return 0;

        uint16_t id = nodeMgr->Set(cFeatures.name, cFeatures);
        if (id > MAX_REGISTERED_CONTENT)
        {
            LogError("Number of registerable nodes (" +
                std::to_string(MAX_REGISTERED_CONTENT + 1) +
                ") exceeded (" + item.name + ")");
        }
    }

    return 0;
}