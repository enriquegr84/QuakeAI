#ifndef HUDDATA_H
#define HUDDATA_H

#include "../QuakeStd.h"

#include "Core/Utility/StringUtil.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

#define HUD_DIR_LEFT_RIGHT 0
#define HUD_DIR_RIGHT_LEFT 1
#define HUD_DIR_TOP_BOTTOM 2
#define HUD_DIR_BOTTOM_TOP 3

#define HUD_CORNER_UPPER  0
#define HUD_CORNER_LOWER  1
#define HUD_CORNER_CENTER 2

// Note that these visibility flags do not determine if the hud items are
// actually drawn, but rather, whether to draw the item should the rest
// of the game state permit it.
#define HUD_FLAG_HEALTH_VISIBLE		(1 << 0)
#define HUD_FLAG_ARMOR_VISIBLE		(1 << 1)
#define HUD_FLAG_AMMO_VISIBLE		(1 << 2)
#define HUD_FLAG_SCORE_VISIBLE		(1 << 3)
#define HUD_FLAG_CROSSHAIR_VISIBLE	(1 << 4)


enum HudElementType 
{
	HUD_ELEM_IMAGE		= 0,
	HUD_ELEM_TEXT		= 1,
	HUD_ELEM_STATBAR	= 2
};

enum HudElementStat 
{
	HUD_STAT_POS = 0,
	HUD_STAT_NAME,
	HUD_STAT_SCALE,
	HUD_STAT_TEXT,
	HUD_STAT_NUMBER,
	HUD_STAT_ITEM,
	HUD_STAT_DIR,
	HUD_STAT_ALIGN,
	HUD_STAT_OFFSET,
	HUD_STAT_WORLD_POS,
	HUD_STAT_SIZE,
	HUD_STAT_Z_INDEX,
	HUD_STAT_TEXT2,
};

struct HudElement
{
    HudElementType type;
    Vector2<float> position;
    std::string name;
    Vector2<float> scale;
    std::string text;
    unsigned int number;
    unsigned int item;
    unsigned int direction;
    Vector2<float> align;
    Vector2<float> offset;
    Vector3<float> worldPosition;
    Vector2<int> size;
    short zIndex = 0;
    std::string text2;
};

extern const EnumString ESHudElementType[];
extern const EnumString ESHudElementStat[];
extern const EnumString ESHudBuiltinElement[];

#endif