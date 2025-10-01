// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef CUSTOMDRAWTARGET_H
#define CUSTOMDRAWTARGET_H

class DrawTarget;

class CustomDrawTarget
{
public:
    // Abstract base class.
    virtual ~CustomDrawTarget();
protected:
    CustomDrawTarget(DrawTarget const* dTarget);

public:
    // Member access.
    inline DrawTarget* GetDrawTarget() const
    {
        return mTarget;
    }

protected:
    DrawTarget* mTarget;
};

#endif
