// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "DummyTransformationNode.h"

#include "Core/OS/Os.h"


//! constructor
DummyTransformationNode::DummyTransformationNode(const ActorId actorId, PVWUpdater* updater)
	: BaseDummyTransformationNode(actorId, updater)
{
	SetCullingMode(CullingMode::CULL_NEVER);
}


//! returns the axis aligned bounding box of this node
BoundingBox<float>& DummyTransformationNode::GetBoundingBox()
{
	return mBoundingBox;
}