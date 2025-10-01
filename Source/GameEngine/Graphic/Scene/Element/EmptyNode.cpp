// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "EmptyNode.h"

#include "Graphic/Scene/Scene.h"


//! constructor
EmptyNode::EmptyNode(const ActorId actorId, PVWUpdater* updater) : Node(actorId, NT_EMPTY)
{
    mPVWUpdater = updater;
}


//! pre render event
bool EmptyNode::PreRender(Scene* pScene)
{
	if (IsVisible())
        pScene->AddToRenderQueue(RP_SOLID, shared_from_this());

    return Node::PreRender(pScene);
}


//! render
bool EmptyNode::Render(Scene* pScene)
{
	// do nothing
    return true;
}


//! returns the axis aligned bounding box of this node
BoundingBox<float>& EmptyNode::GetBoundingBox()
{
    return mBoundingBox;
}