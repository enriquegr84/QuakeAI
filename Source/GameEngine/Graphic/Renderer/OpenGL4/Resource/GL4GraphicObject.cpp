// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)


#include "GL4GraphicObject.h"

GL4GraphicObject::GL4GraphicObject(GraphicObject const* gObject)
    : CustomGraphicObject(gObject), mGLHandle(0)
{
}

void GL4GraphicObject::SetName(std::string const& name)
{
    // TODO:  Determine how to tag OpenGL objects with names?
    mName = name;
}