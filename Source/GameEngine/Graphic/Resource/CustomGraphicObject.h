// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2024
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 6.0.2022.01.06

#ifndef CUSTOMGRAPHICOBJECT_H
#define CUSTOMGRAPHICOBJECT_H

#include "GraphicObject.h"

class CustomGraphicObject
{
public:
    // Abstract base class.
    virtual ~CustomGraphicObject();
protected:
    CustomGraphicObject(GraphicObject const* gObject);

public:
    // Member access.
    inline GraphicObject* GetGraphicsObject() const
    {
        return mGObject;
    }

    // Support for debugging.
    virtual void SetName(std::string const& name) = 0;

    inline std::string const& GetName() const
    {
        return mName;
    }

protected:
    GraphicObject* mGObject;
    std::string mName;
};

#endif