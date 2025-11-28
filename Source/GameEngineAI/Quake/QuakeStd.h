
#ifndef QUAKESTD_H
#define QUAKESTD_H

#include "GameEngineStd.h"

//#undef PHYSX

// angle indexes
#define	AXIS_X	0
#define	AXIS_Z	1
#define	AXIS_Y	2

//ai values
#define MAX_CLUSTERS 600
#define MAX_RANGE_CLUSTERS 400

// physics character actor settings
#define PUSHTRIGGER_FALL_SPEED_Y 3200.f
#define PUSHTRIGGER_FALL_SPEED_XZ 100.f
#define PUSHTRIGGER_JUMP_SPEED_Y 3.2f
#define PUSHTRIGGER_JUMP_SPEED_XZ 1.f
#define DEFAULT_FALL_SPEED_Y 1500.f
#define DEFAULT_FALL_SPEED_XZ 200.f
#define DEFAULT_JUMP_SPEED_Y 1.6f
#define DEFAULT_JUMP_SPEED_XZ 2.f
#define DEFAULT_MOVE_SPEED 1.5f


class ModError : public BaseException
{
public:
    ModError(const std::string& s) : BaseException(s) {}
};

#endif