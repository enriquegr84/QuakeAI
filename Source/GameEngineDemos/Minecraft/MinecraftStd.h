
#ifndef MINECRAFTSTD_H
#define MINECRAFTSTD_H

#include "GameEngineStd.h"

// angle indexes
#define	AXIS_X	0		// left / right
#define	AXIS_Z	2		// forward/ backward
#define	AXIS_Y	1

// This many blocks are sent when player is building
#define LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS 0
// Override for the previous one when distance of block is very low
#define BLOCK_SEND_DISABLE_LIMITS_MAX_D 1

// The absolute working limit is (2^15 - viewing_range).
// I really don't want to make every algorithm to check if it's going near
// the limit or not, so this is lower.
// This is the maximum value the setting map_generation_limit can be
#define MAX_MAP_GENERATION_LIMIT (31000)

// Size of node in floating-point units
// The original idea behind this is to disallow plain casts between
// floating-point and integer positions, which potentially give wrong
// results. (negative coordinates, values between nodes, ...)
#define BS 10.0f

// Dimension of a MapBlock
#define MAP_BLOCKSIZE 16

// Player step height in nodes
#define PLAYER_DEFAULT_STEPHEIGHT 0.6f

// Default maximal breath of a player
#define PLAYER_MAX_BREATH_DEFAULT 10


class ModError : public BaseException 
{
public:
    ModError(const std::string& s) : BaseException(s) {}
};

class InvalidNoiseParamsException : public BaseException 
{
public:
    InvalidNoiseParamsException() :
        BaseException("One or more noise parameters were invalid or require "
            "too much memory")
    {}

    InvalidNoiseParamsException(const std::string &s) : BaseException(s){}
};


#endif