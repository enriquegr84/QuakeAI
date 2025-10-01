#include "Huddata.h"

#include "Core/Utility/StringUtil.h"

const struct EnumString ESHudElementType[] =
{
	{HUD_ELEM_IMAGE,	"image"},
	{HUD_ELEM_TEXT,		"text"},
	{HUD_ELEM_STATBAR,	"stat"},
	{0, NULL}
};

const struct EnumString ESHudElementStat[] =
{
	{HUD_STAT_POS,    "position"},
	{HUD_STAT_POS,    "pos"}, /* Deprecated, only for compatibility's sake */
	{HUD_STAT_NAME,   "name"},
	{HUD_STAT_SCALE,  "scale"},
	{HUD_STAT_TEXT,   "text"},
	{HUD_STAT_NUMBER, "number"},
	{HUD_STAT_ITEM,   "item"},
	{HUD_STAT_ITEM,   "precision"},
	{HUD_STAT_DIR,    "direction"},
	{HUD_STAT_ALIGN,  "alignment"},
	{HUD_STAT_OFFSET, "offset"},
	{HUD_STAT_WORLD_POS, "worldPos"},
	{HUD_STAT_SIZE,    "size"},
	{HUD_STAT_Z_INDEX, "zIndex"},
	{HUD_STAT_TEXT2,   "text2"},
	{0, NULL}
};

const struct EnumString ESHudBuiltinElement[] =
{
	{HUD_FLAG_HEALTH_VISIBLE,		"health"},
	{HUD_FLAG_ARMOR_VISIBLE,		"armor"},
	{HUD_FLAG_AMMO_VISIBLE,			"ammo"},
	{HUD_FLAG_SCORE_VISIBLE,		"score"},
	{HUD_FLAG_CROSSHAIR_VISIBLE,	"crosshair"},
	{0, NULL}
};