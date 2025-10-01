// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef DUMMY_TRANSFORMATION_NODE_H
#define DUMMY_TRANSFORMATION_NODE_H

#include "GameEngineStd.h"

#include "Graphic/Scene/Hierarchy/BoundingBox.h"
#include "Graphic/Scene/Hierarchy/Node.h"

//! Dummy scene node for adding additional transformations to the scene graph.
/** This scene node does not render itself, and does not respond to set/getPosition,
set/getRotation and set/getScale. Its just a simple scene node that takes a
matrix as relative transformation, making it possible to insert any transformation
anywhere into the scene graph.
This scene node is for example used by the IAnimatedMeshSceneNode for emulating
joint scene nodes when playing skeletal animations.
*/
class BaseDummyTransformationNode : public Node
{
public:

    //! Constructor
    BaseDummyTransformationNode(const ActorId actorId, PVWUpdater* updater)
        : Node(actorId, NT_DUMMY_TRANSFORMATION) {}

};

class DummyTransformationNode : public BaseDummyTransformationNode
{
public:

	//! constructor
    DummyTransformationNode(const ActorId actorId, PVWUpdater* updater);

    //! returns the axis aligned bounding box of this node
    virtual BoundingBox<float>& GetBoundingBox();

	//! does nothing.
	virtual void Render() {}

	//! Returns type of the scene node
	virtual NodeType GetType() const { return NT_DUMMY_TRANSFORMATION; }


private:

	BoundingBox<float> mBoundingBox;
};


#endif

