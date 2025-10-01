// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef GL4GRAPHICOBJECT_H
#define GL4GRAPHICOBJECT_H

#include "Graphic/Resource/CustomGraphicObject.h"
#include "Graphic/Renderer/OpenGL4/OpenGL.h"

class GRAPHIC_ITEM GL4GraphicObject : public CustomGraphicObject
{
public:
    // Abstract base class.
    virtual ~GL4GraphicObject() = default;
protected:
    GL4GraphicObject(GraphicObject const* gObject);

public:
    // Member access.
    inline GLuint GetGLHandle() const;

    // Support for debugging.
    virtual void SetName(std::string const& name) override;

protected:

    GLuint mGLHandle;
};

inline GLuint GL4GraphicObject::GetGLHandle() const
{
    return mGLHandle;
}

#endif