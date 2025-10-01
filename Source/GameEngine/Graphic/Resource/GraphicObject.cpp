// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include "GraphicObject.h"

GraphicObject::~GraphicObject()
{
    msLFDMutex.lock();
    {
        for (auto const& listener : msLFDSet)
        {
            listener->OnDestroy(this);
        }
    }
    msLFDMutex.unlock();
}

GraphicObject::GraphicObject()
    :
    mType(GE_GRAPHICS_OBJECT),
    mName(L"")
{
}

GraphicObject::GraphicObject(GraphicObjectType type)
    :
    mType(type),
    mName(L"")
{
}

void GraphicObject::SubscribeForDestruction(std::shared_ptr<ListenerForDestruction> const& listener)
{
    msLFDMutex.lock();
    {
        msLFDSet.insert(listener);
    }
    msLFDMutex.unlock();
}

void GraphicObject::UnsubscribeForDestruction(std::shared_ptr<ListenerForDestruction> const& listener)
{
    msLFDMutex.lock();
    {
        msLFDSet.erase(listener);
    }
    msLFDMutex.unlock();
}

std::mutex GraphicObject::msLFDMutex{};
std::set<std::shared_ptr<GraphicObject::ListenerForDestruction>> GraphicObject::msLFDSet{};
