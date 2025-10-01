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

#include "MapNode.h"

#include "ContentMapNode.h"
#include "Map.h"

#include "../../Graphics/Node.h"

#include "Core/Utility/Serialize.h"


static const uint8_t WallmountedToFacedir[6] = {
    20, 0, 16 + 1, 12 + 3, 8, 4 + 2};

static const RotationDegrees WallmountedToRot[] = {
    ROTATE_0, ROTATE_180, ROTATE_90, ROTATE_270 };

static const uint8_t RotToWallmounted[] = {
    2, 4, 3, 5 };

/*
	MapNode
*/
void MapNode::GetColor(const ContentFeatures& f, SColor* color) const
{
	if (f.palette) 
    {
		*color = (*f.palette)[param2];
		return;
	}
	*color = f.color;
}

void MapNode::SetLight(LightBank bank, uint8_t aLight, const ContentFeatures& f) noexcept
{
	// If node doesn't contain light data, ignore this
	if(f.paramType != CPT_LIGHT)
		return;
	if(bank == LIGHTBANK_DAY)
	{
		param1 &= 0xf0;
		param1 |= aLight & 0x0f;
	}
	else if(bank == LIGHTBANK_NIGHT)
	{
		param1 &= 0x0f;
		param1 |= (aLight & 0x0f)<<4;
	}
	else LogAssert(NULL, "Invalid light bank");
}

void MapNode::SetLight(LightBank bank, uint8_t aLight, const NodeManager* nodeMgr)
{
	SetLight(bank, aLight, nodeMgr->Get(*this));
}

bool MapNode::IsLightDayNightEq(const NodeManager* nodeMgr) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	bool isEqual;

	if (f.paramType == CPT_LIGHT) 
    {
		uint8_t day = std::max(f.lightSource, (uint8_t)(param1 & 0x0f));
		uint8_t night = std::max(f.lightSource, (uint8_t)((param1 >> 4) & 0x0f));
		isEqual = day == night;
	} 
    else 
    {
		isEqual = true;
	}

	return isEqual;
}

uint8_t MapNode::GetLight(LightBank bank, const NodeManager* nodeMgr) const
{
	// Select the brightest of [light source, propagated light]
	const ContentFeatures& f = nodeMgr->Get(*this);

	uint8_t light;
	if(f.paramType == CPT_LIGHT)
		light = bank == LIGHTBANK_DAY ? param1 & 0x0f : (param1 >> 4) & 0x0f;
	else
		light = 0;

	return std::max(f.lightSource, light);
}

uint8_t MapNode::GetLightRaw(LightBank bank, const ContentFeatures& f) const noexcept
{
	if(f.paramType == CPT_LIGHT)
		return bank == LIGHTBANK_DAY ? param1 & 0x0f : (param1 >> 4) & 0x0f;
	return 0;
}

uint8_t MapNode::GetLightNoChecks(LightBank bank, const ContentFeatures* f) const noexcept
{
	return std::max(f->lightSource, bank == LIGHTBANK_DAY ? 
        (uint8_t)(param1 & 0x0f) : (uint8_t)((param1 >> 4) & 0x0f));
}

bool MapNode::GetLightBanks(uint8_t& lightDay, uint8_t& lightNight, const NodeManager* nodeMgr) const
{
	// Select the brightest of [light source, propagated light]
	const ContentFeatures& f = nodeMgr->Get(*this);
	if(f.paramType == CPT_LIGHT)
	{
		lightDay = param1 & 0x0f;
		lightNight = (param1>>4)&0x0f;
	}
	else
	{
		lightDay = 0;
		lightNight = 0;
	}
	if(f.lightSource > lightDay)
		lightDay = f.lightSource;
	if(f.lightSource > lightNight)
		lightNight = f.lightSource;
	return f.paramType == CPT_LIGHT || f.lightSource != 0;
}

uint8_t MapNode::GetFaceDir(const NodeManager* nodeMgr, bool allowWallmounted) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	if (f.paramType2 == CPT2_FACEDIR || f.paramType2 == CPT2_COLORED_FACEDIR)
		return (GetParam2() & 0x1F) % 24;
	if (allowWallmounted && 
        (f.paramType2 == CPT2_WALLMOUNTED || f.paramType2 == CPT2_COLORED_WALLMOUNTED))
		return WallmountedToFacedir[GetParam2() & 0x07];
	return 0;
}

uint8_t MapNode::GetWallMounted(const NodeManager* nodeMgr) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	if (f.paramType2 == CPT2_WALLMOUNTED || f.paramType2 == CPT2_COLORED_WALLMOUNTED)
		return GetParam2() & 0x07;
	return 0;
}

Vector3<short> MapNode::GetWallMountedDir(const NodeManager* nodeMgr) const
{
	switch(GetWallMounted(nodeMgr))
	{
        case 0: default: return Vector3<short>{0, 1, 0};
        case 1: return Vector3<short>{0, -1, 0};
        case 2: return Vector3<short>{1, 0, 0};
        case 3: return Vector3<short>{-1, 0, 0};
        case 4: return Vector3<short>{0, 0, 1};
        case 5: return Vector3<short>{0, 0, -1};
	}
}

uint8_t MapNode::GetDegRotate(const NodeManager* nodeMgr) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	if (f.paramType2 == CPT2_DEGROTATE)
		return GetParam2() % 240;
	if (f.paramType2 == CPT2_COLORED_DEGROTATE)
		return 10 * ((GetParam2() & 0x1F) % 24);
	return 0;
}

void MapNode::RotateAlongYAxis(const NodeManager* nodeMgr, RotationDegrees rot)
{
	ContentParamType2 cpt2 = nodeMgr->Get(*this).paramType2;

	if (cpt2 == CPT2_FACEDIR || cpt2 == CPT2_COLORED_FACEDIR) 
    {
		static const uint8_t rotateFaceDir[24 * 4] = {
			// Table value = rotated faceDir
			// Columns: 0, 90, 180, 270 degrees rotation around vertical axis
			// Rotation is anticlockwise as seen from above (+Y)

			0, 1, 2, 3,  // Initial faceDir 0 to 3
			1, 2, 3, 0,
			2, 3, 0, 1,
			3, 0, 1, 2,

			4, 13, 10, 19,  // 4 to 7
			5, 14, 11, 16,
			6, 15, 8, 17,
			7, 12, 9, 18,

			8, 17, 6, 15,  // 8 to 11
			9, 18, 7, 12,
			10, 19, 4, 13,
			11, 16, 5, 14,

			12, 9, 18, 7,  // 12 to 15
			13, 10, 19, 4,
			14, 11, 16, 5,
			15, 8, 17, 6,

			16, 5, 14, 11,  // 16 to 19
			17, 6, 15, 8,
			18, 7, 12, 9,
			19, 4, 13, 10,

			20, 23, 22, 21,  // 20 to 23
			21, 20, 23, 22,
			22, 21, 20, 23,
			23, 22, 21, 20
		};
		uint8_t faceDir = (param2 & 31) % 24;
		uint8_t index = faceDir * 4 + rot;
		param2 &= ~31;
		param2 |= rotateFaceDir[index];
	} 
    else if (cpt2 == CPT2_WALLMOUNTED || cpt2 == CPT2_COLORED_WALLMOUNTED) 
    {
		uint8_t wmountface = (param2 & 7);
		if (wmountface <= 1)
			return;

        RotationDegrees oldrot = WallmountedToRot[wmountface - 2];
		param2 &= ~7;
		param2 |= RotToWallmounted[(oldrot - rot) & 3];
	} 
    else if (cpt2 == CPT2_DEGROTATE) 
    {
		int angle = param2; // in 1.5°
		angle += 60 * rot; // don’t do that on uint8_t
		angle %= 240;
		param2 = angle;
	} 
    else if (cpt2 == CPT2_COLORED_DEGROTATE) 
    {
		int angle = param2 & 0x1F; // in 15°
		int color = param2 & 0xE0;
		angle += 6 * rot;
		angle %= 24;
		param2 = color | angle;
	}
}

void TransformNodeBox(const MapNode& n, const NodeBox& nodebox,
	const NodeManager *nodeMgr, std::vector<BoundingBox<float>> *pBoxes, uint8_t neighbors = 0)
{
	std::vector<BoundingBox<float>>& bBoxes = *pBoxes;

	if (nodebox.type == NODEBOX_FIXED || nodebox.type == NODEBOX_LEVELED) 
    {
		const std::vector<BoundingBox<float>>& fixed = nodebox.fixed;
		int faceDir = n.GetFaceDir(nodeMgr, true);
		uint8_t axisdir = faceDir>>2;
		faceDir&=0x03;
		for (BoundingBox<float> box : fixed) 
        {
			if (nodebox.type == NODEBOX_LEVELED)
				box.mMaxEdge[1] = (-0.5f + n.GetLevel(nodeMgr) / 64.0f) * BS;

            Quaternion<float> tgt;
			switch (axisdir) 
            {
			    case 0:
				    if(faceDir == 1)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXZBy(-90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXZBy(-90);
				    }
				    else if(faceDir == 2)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXZBy(180);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXZBy(180);
				    }
				    else if(faceDir == 3)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y),90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXZBy(90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXZBy(90);
				    }
				    break;
			    case 1: // z+
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                    box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                    //box.mMinEdge.RotateYZBy(90);

                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                    box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                    //box.mMaxEdge.RotateYZBy(90);

				    if(faceDir == 1)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXYBy(90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXYBy(90);
				    }
				    else if(faceDir == 2)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXYBy(180);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXYBy(180);
				    }
				    else if(faceDir == 3)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXYBy(-90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXYBy(-90);
				    }
				    break;
			    case 2: //z-
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                    box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                    //box.mMinEdge.RotateYZBy(-90);

                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X),  90 * (float)GE_C_DEG_TO_RAD));
                    box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                    //box.mMaxEdge.RotateYZBy(-90);

				    if(faceDir == 1)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXYBy(-90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXYBy(-90);
				    }
				    else if(faceDir == 2)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXYBy(180);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXYBy(180);
				    }
				    else if(faceDir == 3)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXYBy(90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXYBy(90);
				    }
				    break;
			    case 3:  //x+
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                    box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                    //box.mMinEdge.RotateXYBy(-90);

                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 90 * (float)GE_C_DEG_TO_RAD));
                    box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                    //box.mMaxEdge.RotateXYBy(-90);
				    if(faceDir == 1)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateYZBy(90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateYZBy(90);
				    }
				    else if(faceDir == 2)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateYZBy(180);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateYZBy(180);
				    }
				    else if(faceDir == 3)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateYZBy(-90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateYZBy(-90);
				    }
				    break;
			    case 4:  //x-
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                    box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                    //box.mMinEdge.RotateXYBy(90);

                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), -90 * (float)GE_C_DEG_TO_RAD));
                    box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                    //box.mMaxEdge.RotateXYBy(90);
				    if(faceDir == 1)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateYZBy(-90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateYZBy(-90);
				    }
				    else if(faceDir == 2)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateYZBy(180);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -180 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateYZBy(180);
				    }
				    else if(faceDir == 3)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X),  -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateYZBy(90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_X), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateYZBy(90);
				    }
				    break;
			    case 5:
                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 180 * (float)GE_C_DEG_TO_RAD));
                    box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                    //box.mMinEdge.RotateXYBy(-180);

                    tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Z), 180 * (float)GE_C_DEG_TO_RAD));
                    box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                    //box.mMaxEdge.RotateXYBy(-180);
				    if(faceDir == 1)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXZBy(90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXZBy(90);
				    }
				    else if(faceDir == 2)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXZBy(180);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXZBy(180);
				    }
				    else if(faceDir == 3)
				    {
                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMinEdge = HProject(Rotate(tgt, HLift(box.mMinEdge, 0.f)));
                        //box.mMinEdge.RotateXZBy(-90);

                        tgt = Rotation<3, float>(
                            AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                        box.mMaxEdge = HProject(Rotate(tgt, HLift(box.mMaxEdge, 0.f)));
                        //box.mMaxEdge.RotateXZBy(-90);
				    }
				    break;
			    default:
				    break;
			}
			box.Repair();
			bBoxes.push_back(box);
		}
	}
	else if(nodebox.type == NODEBOX_WALLMOUNTED)
	{
		Vector3<short> dir = n.GetWallMountedDir(nodeMgr);

		// top
        if (dir == Vector3<short>{0, 1, 0})
		{
			bBoxes.push_back(nodebox.wallTop);
		}
		// bottom
        else if (dir == Vector3<short>{0, -1, 0})
		{
			bBoxes.push_back(nodebox.wallBottom);
		}
		// side
		else
		{
			Vector3<float> vertices[2] =
			{
				nodebox.wallSide.mMinEdge,
				nodebox.wallSide.mMaxEdge
			};

			for (Vector3<float> &vertex : vertices) 
            {
                if (dir == Vector3<short>{-1, 0, 0})
                { 
                    Quaternion<float> tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 0 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                    //vertex.RotateXZBy(0);
                }
                if (dir == Vector3<short>{1, 0, 0})
                { 
                    Quaternion<float> tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 180 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                    //vertex.RotateXZBy(180);
                }
                if (dir == Vector3<short>{0, 0, -1})
                { 
                    Quaternion<float> tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), 90 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                    //vertex.RotateXZBy(90);
                }
                if (dir == Vector3<short>{0, 0, 1})
                { 
                    Quaternion<float> tgt = Rotation<3, float>(
                        AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y), -90 * (float)GE_C_DEG_TO_RAD));
                    vertex = HProject(Rotate(tgt, HLift(vertex, 0.f)));
                    //vertex.RotateXZBy(-90);
                }

			}

			BoundingBox<float> bBox = BoundingBox<float>(vertices[0]);
			bBox.GrowToContain(vertices[1]);
			bBoxes.push_back(bBox);
		}
	}
	else if (nodebox.type == NODEBOX_CONNECTED)
	{
		size_t boxesSize = bBoxes.size();
		boxesSize += nodebox.fixed.size();
		if (neighbors & 1)
			boxesSize += nodebox.connectTop.size();
		else
			boxesSize += nodebox.disconnectedTop.size();

		if (neighbors & 2)
			boxesSize += nodebox.connectBottom.size();
		else
			boxesSize += nodebox.disconnectedBottom.size();

		if (neighbors & 4)
			boxesSize += nodebox.connectFront.size();
		else
			boxesSize += nodebox.disconnectedFront.size();

		if (neighbors & 8)
			boxesSize += nodebox.connectLeft.size();
		else
			boxesSize += nodebox.disconnectedLeft.size();

		if (neighbors & 16)
			boxesSize += nodebox.connectBack.size();
		else
			boxesSize += nodebox.disconnectedBack.size();

		if (neighbors & 32)
			boxesSize += nodebox.connectRight.size();
		else
			boxesSize += nodebox.disconnectedRight.size();

		if (neighbors == 0)
			boxesSize += nodebox.disconnected.size();

		if (neighbors < 4)
			boxesSize += nodebox.disconnectedSides.size();

		bBoxes.reserve(boxesSize);

#define BOXESPUSHBACK(c) \
		for (std::vector<BoundingBox<float>>::const_iterator \
				it = (c).begin(); \
				it != (c).end(); ++it) \
			(bBoxes).push_back(*it);

		BOXESPUSHBACK(nodebox.fixed);

		if (neighbors & 1) 
        {
			BOXESPUSHBACK(nodebox.connectTop);
		}
        else 
        {
			BOXESPUSHBACK(nodebox.disconnectedTop);
		}

		if (neighbors & 2) 
        {
			BOXESPUSHBACK(nodebox.connectBottom);
		}
        else 
        {
			BOXESPUSHBACK(nodebox.disconnectedBottom);
		}

		if (neighbors & 4) 
        {
			BOXESPUSHBACK(nodebox.connectFront);
		} 
        else 
        {
			BOXESPUSHBACK(nodebox.disconnectedFront);
		}

		if (neighbors & 8) 
        {
			BOXESPUSHBACK(nodebox.connectLeft);
		} 
        else 
        {
			BOXESPUSHBACK(nodebox.disconnectedLeft);
		}

		if (neighbors & 16) 
        {
			BOXESPUSHBACK(nodebox.connectBack);
		} 
        else 
        {
			BOXESPUSHBACK(nodebox.disconnectedBack);
		}

		if (neighbors & 32) 
        {
			BOXESPUSHBACK(nodebox.connectRight);
		} 
        else
        {
			BOXESPUSHBACK(nodebox.disconnectedRight);
		}

		if (neighbors == 0) 
        {
			BOXESPUSHBACK(nodebox.disconnected);
		}

		if (neighbors < 4) 
        {
			BOXESPUSHBACK(nodebox.disconnectedSides);
		}

	}
	else // NODEBOX_REGULAR
	{
	bBoxes.emplace_back(-BS/2,-BS/2,-BS/2,BS/2,BS/2,BS/2);
	}
}

static inline void GetNeighborConnectingFace(
	const Vector3<short>& p, const NodeManager* nodedef,
	std::shared_ptr<Map> map, MapNode n, uint8_t bitmask, uint8_t* neighbors)
{
	MapNode n2 = map->GetNode(p);
	if (nodedef->NodeboxConnects(n, n2, bitmask))
		*neighbors |= bitmask;
}

uint8_t MapNode::GetNeighbors(Vector3<short> pos, std::shared_ptr<Map> map) const
{
	const NodeManager* nodeMgr = map->GetNodeManager();
	uint8_t neighbors = 0;
	const ContentFeatures& f = nodeMgr->Get(*this);
	// locate possible neighboring nodes to connect to
	if (f.drawType == NDT_NODEBOX && f.nodeBox.type == NODEBOX_CONNECTED) 
    {
		Vector3<short> pos2 = pos;

        pos2[1]++;
		GetNeighborConnectingFace(pos2, nodeMgr, map, *this, 1, &neighbors);

        pos2 = pos;
        pos2[1]--;
		GetNeighborConnectingFace(pos2, nodeMgr, map, *this, 2, &neighbors);

        pos2 = pos;
        pos2[2]--;
		GetNeighborConnectingFace(pos2, nodeMgr, map, *this, 4, &neighbors);

        pos2 = pos;
        pos2[0]--;
		GetNeighborConnectingFace(pos2, nodeMgr, map, *this, 8, &neighbors);

        pos2 = pos;
        pos2[2]++;
		GetNeighborConnectingFace(pos2, nodeMgr, map, *this, 16, &neighbors);

        pos2 = pos;
        pos2[0]++;
		GetNeighborConnectingFace(pos2, nodeMgr, map, *this, 32, &neighbors);
	}

	return neighbors;
}

void MapNode::GetNodeBoxes(const NodeManager* nodeMgr,
	std::vector<BoundingBox<float>>* boxes, uint8_t neighbors) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	TransformNodeBox(*this, f.nodeBox, nodeMgr, boxes, neighbors);
}

void MapNode::GetCollisionBoxes(const NodeManager* nodeMgr,
	std::vector<BoundingBox<float>>* boxes, uint8_t neighbors) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	if (f.collisionBox.fixed.empty())
		TransformNodeBox(*this, f.nodeBox, nodeMgr, boxes, neighbors);
	else
		TransformNodeBox(*this, f.collisionBox, nodeMgr, boxes, neighbors);
}

void MapNode::GetSelectionBoxes(const NodeManager* nodeMgr,
	std::vector<BoundingBox<float>>* boxes, uint8_t neighbors) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	TransformNodeBox(*this, f.selectionBox, nodeMgr, boxes, neighbors);
}

uint8_t MapNode::GetMaxLevel(const NodeManager* nodeMgr) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	// todo: after update in all games leave only if (f.paramType2 ==
	if( f.liquidType == LIQUID_FLOWING || f.paramType2 == CPT2_FLOWINGLIQUID)
		return LIQUID_LEVEL_MAX;
	if(f.leveled || f.paramType2 == CPT2_LEVELED)
		return f.leveledMax;
	return 0;
}

uint8_t MapNode::GetLevel(const NodeManager* nodeMgr) const
{
	const ContentFeatures& f = nodeMgr->Get(*this);
	// todo: after update in all games leave only if (f.paramType2 ==
	if(f.liquidType == LIQUID_SOURCE)
		return LIQUID_LEVEL_SOURCE;
	if (f.paramType2 == CPT2_FLOWINGLIQUID)
		return GetParam2() & LIQUID_LEVEL_MASK;
	if(f.liquidType == LIQUID_FLOWING) // can remove if all paramType2 setted
		return GetParam2() & LIQUID_LEVEL_MASK;
	if (f.paramType2 == CPT2_LEVELED) {
		uint8_t level = GetParam2() & LEVELED_MASK;
		if (level)
			return level;
	}
	// Return static value from nodemgr if param2 isn't used for level
	if (f.leveled > f.leveledMax)
		return f.leveledMax;
	return f.leveled;
}

int8_t MapNode::SetLevel(const NodeManager* nodeMgr, short level)
{
	int8_t rest = 0;
	const ContentFeatures& f = nodeMgr->Get(*this);
	if (f.paramType2 == CPT2_FLOWINGLIQUID || 
        f.liquidType == LIQUID_FLOWING || 
        f.liquidType == LIQUID_SOURCE)
    {
		if (level <= 0) 
        { // liquid can’t exist with zero level
			SetContent(CONTENT_AIR);
			return 0;
		}
		if (level >= LIQUID_LEVEL_SOURCE) 
        {
			rest = level - LIQUID_LEVEL_SOURCE;
			SetContent(f.liquidAlternativeSourceId);
			SetParam2(0);
		} 
        else 
        {
			SetContent(f.liquidAlternativeFlowingId);
			SetParam2((level & LIQUID_LEVEL_MASK) | (GetParam2() & ~LIQUID_LEVEL_MASK));
		}
	} else if (f.paramType2 == CPT2_LEVELED) 
    {
		if (level < 0) 
        { // zero means default for a leveled nodebox
			rest = (int8_t)level;
			level = 0;
		} 
        else if (level > f.leveledMax) 
        {
			rest = level - f.leveledMax;
			level = f.leveledMax;
		}
		SetParam2((level & LEVELED_MASK) | (GetParam2() & ~LEVELED_MASK));
	}
	return rest;
}

int8_t MapNode::AddLevel(const NodeManager* nodeMgr, short add)
{
	short level = GetLevel(nodeMgr);
	level += add;
	return SetLevel(nodeMgr, level);
}

unsigned int MapNode::SerializedLength(uint8_t version)
{
	if(!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	if (version == 0)
		return 1;

	if (version <= 9)
		return 2;

	if (version <= 23)
		return 3;

	return 4;
}
void MapNode::Serialize(uint8_t* dest, uint8_t version) const
{
	if(!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	// Can't do this anymore; we have 16-bit dynamically allocated node IDs
	// in memory; conversion just won't work in this direction.
	if(version < 24)
		throw SerializationError("MapNode::Serialize: serialization to version < 24 not possible");

	WriteUInt16(dest+0, param0);
	WriteUInt8(dest+2, param1);
	WriteUInt8(dest+3, param2);
}
void MapNode::Deserialize(uint8_t* source, uint8_t version)
{
	if(!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	if(version <= 21)
	{
		DeserializePre22(source, version);
		return;
	}

	if(version >= 24)
    {
		param0 = ReadUInt16(source+0);
		param1 = ReadUInt8(source+2);
		param2 = ReadUInt8(source+3);
	}
    else
    {
		param0 = ReadUInt8(source+0);
		param1 = ReadUInt8(source+1);
		param2 = ReadUInt8(source+2);
		if(param0 > 0x7F)
        {
			param0 |= ((param2&0xF0)<<4);
			param2 &= 0x0F;
		}
	}
}
void MapNode::SerializeBulk(std::ostream& os, int version, 
    const MapNode* nodes, unsigned int mNodeCount,
    uint8_t contentWidth, uint8_t paramsWidth, int compressionLevel)
{
	if (!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	LogAssert(contentWidth == 2, "invalid content width");
    LogAssert(paramsWidth == 2, "invalid params width");

	// Can't do this anymore; we have 16-bit dynamically allocated node IDs
	// in memory; conversion just won't work in this direction.
	if (version < 24)
		throw SerializationError("MapNode::SerializeBulk: serialization to version < 24 not possible");

	size_t dataBufSize = mNodeCount * (contentWidth + paramsWidth);
	uint8_t* dataBuf = new uint8_t[dataBufSize];

	unsigned int start1 = contentWidth * mNodeCount;
	unsigned int start2 = (contentWidth + 1) * mNodeCount;

	// Serialize content
	for (unsigned int i = 0; i < mNodeCount; i++) 
    {
		WriteUInt16(&dataBuf[i * 2], nodes[i].param0);
		WriteUInt8(&dataBuf[start1 + i], nodes[i].param1);
		WriteUInt8(&dataBuf[start2 + i], nodes[i].param2);
	}

	/*
		Compress data to output stream
	*/

	CompressZlib(dataBuf, dataBufSize, os, compressionLevel);

	delete [] dataBuf;
}

// Deserialize bulk node data
void MapNode::DeserializeBulk(std::istream& is, int version, MapNode* nodes, 
	unsigned int nodeCount, uint8_t contentWidth, uint8_t paramsWidth)
{
	if(!VersionSupported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	if (version < 22 || (contentWidth != 1 && contentWidth != 2) || paramsWidth != 2)
		LogError("Deserialize bulk node data error");

	// Uncompress or read data
	unsigned int len = nodeCount * (contentWidth + paramsWidth);
	std::ostringstream os(std::ios_base::binary);
	DecompressZlib(is, os);
	std::string str = os.str();
	if(str.size() != len)
		throw SerializationError("DeserializeBulkNodes: decompress resulted in invalid size");
	const uint8_t* dataBuf = reinterpret_cast<const uint8_t*>(str.c_str());

	// Deserialize content
	if(contentWidth == 1)
	{
		for(unsigned int i=0; i< nodeCount; i++)
			nodes[i].param0 = ReadUInt8(&dataBuf[i]);
	}
	else if(contentWidth == 2)
	{
		for(unsigned int i=0; i< nodeCount; i++)
			nodes[i].param0 = ReadUInt16(&dataBuf[i*2]);
	}

	// Deserialize param1
	unsigned int start1 = contentWidth * nodeCount;
	for(unsigned int i=0; i< nodeCount; i++)
		nodes[i].param1 = ReadUInt8(&dataBuf[start1 + i]);

	// Deserialize param2
	unsigned int start2 = (contentWidth + 1) * nodeCount;
	if(contentWidth == 1)
	{
		for(unsigned int i=0; i< nodeCount; i++)
        {
			nodes[i].param2 = ReadUInt8(&dataBuf[start2 + i]);
			if(nodes[i].param0 > 0x7F)
            {
				nodes[i].param0 <<= 4;
				nodes[i].param0 |= (nodes[i].param2&0xF0)>>4;
				nodes[i].param2 &= 0x0F;
			}
		}
	}
	else if(contentWidth == 2)
	{
		for(unsigned int i=0; i< nodeCount; i++)
			nodes[i].param2 = ReadUInt8(&dataBuf[start2 + i]);
	}
}

/*
	Legacy serialization
*/
void MapNode::DeserializePre22(const uint8_t* source, uint8_t version)
{
	if(version <= 1)
	{
		param0 = source[0];
	}
	else if(version <= 9)
	{
		param0 = source[0];
		param1 = source[1];
	}
	else
	{
		param0 = source[0];
		param1 = source[1];
		param2 = source[2];
		if(param0 > 0x7f)
        {
			param0 <<= 4;
			param0 |= (param2&0xf0)>>4;
			param2 &= 0x0f;
		}
	}

	// Convert special values from old version to new
	if(version <= 19)
	{
		// In these versions, CONTENT_IGNORE and CONTENT_AIR
		// are 255 and 254
		// Version 19 is messed up with sometimes the old values and sometimes not
		if(param0 == 255)
			param0 = CONTENT_IGNORE;
		else if(param0 == 254)
			param0 = CONTENT_AIR;
	}

	// Translate to our known version
	*this = MapNodeTranslateToInternal(*this, version);
}