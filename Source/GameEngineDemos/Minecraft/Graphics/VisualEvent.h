/*
Minetest
Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#ifndef VISUALEVENT_H
#define VISUALEVENT_H

#include "GameEngineStd.h"

#include "../Data/Huddata.h"

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Algebra/Vector3.h"

struct ParticleParameters;
struct ParticleSpawnerParameters;
struct SkyboxParams;
struct SunParams;
struct MoonParams;
struct StarParams;

enum VisualEventType
{
	VE_NONE = 0,
	VE_PLAYER_DAMAGE,
	VE_PLAYER_FORCE_MOVE,
	VE_DEATHSCREEN,
	VE_SHOW_FORM,
	VE_SHOW_LOCAL_FORM,
	VE_SPAWN_PARTICLE,
	VE_ADD_PARTICLESPAWNER,
	VE_DELETE_PARTICLESPAWNER,
	VE_HUDADD,
	VE_HUDRM,
	VE_HUDCHANGE,
	VE_SET_SKY,
	VE_SET_SUN,
	VE_SET_MOON,
	VE_SET_STARS,
	VE_OVERRIDE_DAY_NIGHT_RATIO,
	VE_CLOUD_PARAMS,
	VE_MAX,
};

struct VisualEventHudAdd
{
	unsigned int id;
	unsigned char type;
	Vector2<float> pos, scale;
	std::string name;
    std::string text, text2;
	unsigned int number, item, dir;
    Vector2<float> align, offset;
    Vector3<float> worldPos;
    Vector2<int> size;
	int16_t zIndex;
};

struct VisualEventHudChange
{
	unsigned int id;
	HudElementStat stat;
	Vector2<float> v2fData;
	std::string sData;
	unsigned int data;
	Vector3<float> v3fData;
	Vector2<int> v2sData;
};

struct VisualEvent
{
	VisualEventType type;
	union
	{
		// struct{
		//} none;
		struct
		{
			uint16_t amount;
		} PlayerDamage;

		struct
		{
			float pitch;
            float yaw;
		} PlayerForceMove;

		struct
		{
			bool setCameraPointTarget;
			float cameraPointTargetX;
            float cameraPointTargetY;
            float cameraPointTargetZ;
		} DeathScreen;

		struct
		{
			std::string* form;
			std::string* formName;
		} ShowForm;

		// struct{
		//} textures_updated;
		ParticleParameters* spawnParticle;
		struct
		{
			ParticleSpawnerParameters* parameters;
			uint16_t attachedId;
			uint64_t id;
		} AddParticleSpawner;

		struct
		{
			unsigned int id;
		} DeleteParticleSpawner;

		VisualEventHudAdd* hudadd;

		struct
		{
			unsigned int id;
		} HudRemove;

		VisualEventHudChange* hudChange;
		SkyboxParams* setSky;
		struct
		{
			bool doOverride;
			float ratio;
		} OverrideDayNightRatio;

		struct
		{
			float density;
			unsigned int colorBright;
            unsigned int colorAmbient;
			float height;
            float thickness;
            float speedX;
            float speedY;
		} CloudParams;

		SunParams* sunParams;
		MoonParams* moonParams;
		StarParams* starParams;
	};
};

#endif