// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef PATHNODE_H
#define PATHNODE_H

#include "AI/Pathing.h"

#include "Graphic/Scene/Hierarchy/Node.h"

class PathNode : public Node
{
public:

	//! constructor
	PathNode(const ActorId actorId, PVWUpdater* updater,
		const std::shared_ptr<Texture2>& texture, Vector3<float> size);

	virtual ~PathNode();

	//! Render events
	virtual bool PreRender(Scene *pScene);
	virtual bool Render(Scene *pScene);

    /** \return Axis aligned bounding box of this buffer. */
    virtual BoundingBox<float>& GetBoundingBox();

	//! Returns type of the scene node
	virtual NodeType GetType() const { return NT_ANY; }

	//! Returns the visual based on the zero based index i. To get the amount 
	//! of visuals used by this scene node, use GetVisualCount(). 
	//! This function is needed for inserting the node into the scene hierarchy 
	//! at an optimal position for minimizing renderstate changes, but can also 
	//! be used to directly modify the visual of a scene node.
	virtual std::shared_ptr<Visual> const& GetVisual(unsigned int i);

	//! return amount of visuals of this scene node.
	virtual size_t GetVisualCount() const;

	//! returns the material based on the zero based index i. To get the amount
	//! of materials used by this scene node, use GetMaterialCount().
	//! This function is needed for inserting the node into the scene hirachy on a
	//! optimal position for minimizing renderstate changes, but can also be used
	//! to directly modify the material of a scene node.
	virtual std::shared_ptr<Material> const& GetMaterial(unsigned int i);

	//! returns amount of materials used by this scene node.
	virtual size_t GetMaterialCount() const;

	//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
	/** \param textureLayer Layer of texture to be set. Must be a
	value smaller than MATERIAL_MAX_TEXTURES.
	\param texture New texture to be used. */
	virtual void SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture);

	//! Sets the material type of all materials in this scene node to a new material type.
	/** \param newType New type of material to be set. */
	virtual void SetMaterialType(MaterialType newType);

	void GenerateMesh(const std::vector<Vector3<float>>& nodes);

private:

	void GenerateGeometry(BaseMeshBuffer* meshBuffer, const std::vector<Vector3<float>>& nodes);

	std::shared_ptr<BlendState> mBlendState;
	std::shared_ptr<DepthStencilState> mDepthStencilState;
	std::shared_ptr<RasterizerState> mRasterizerState;

	std::shared_ptr<Visual> mVisual;

	std::shared_ptr<Texture2> mTexture;
	std::shared_ptr<BaseMesh> mMesh;
	Vector3<float> mSize;
};


#endif

