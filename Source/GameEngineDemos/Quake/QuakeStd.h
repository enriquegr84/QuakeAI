
#ifndef QUAKESTD_H
#define QUAKESTD_H

#include "GameEngineStd.h"

// angle indexes
#define	AXIS_X	0
#define	AXIS_Z	1
#define	AXIS_Y	2


class ModError : public BaseException
{
public:
    ModError(const std::string& s) : BaseException(s) {}
};

#endif