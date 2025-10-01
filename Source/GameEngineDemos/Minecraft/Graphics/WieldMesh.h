/*
Minetest
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef WIELDMESH_H
#define WIELDMESH_H

#include "GameEngineStd.h"

#include "../Games/Environment/VisualEnvironment.h"

#include "Graphic/Scene/Scene.h"
#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"

#include "Graphic/Scene/Element/MeshNode.h"

#include "Graphic/Scene/Mesh/Mesh.h"

struct ItemStack;
struct ContentFeatures;

class ExtrusionMeshCache;

#define MIN_EXTRUSION_MESH_RESOLUTION 16
#define MAX_EXTRUSION_MESH_RESOLUTION 512

/*!
 * Holds color information of an item mesh's buffer.
 */
struct ItemPartColor
{
	/*!
	 * If this is false, the global base color of the item
	 * will be used instead of the specific color of the
	 * buffer.
	 */
	bool overrideBase = false;
	/*!
	 * The color of the buffer.
	 */
	SColor color = 0;

	ItemPartColor() = default;

	ItemPartColor(bool override, SColor c) : overrideBase(override), color(c)
	{

	}
};

struct ItemMesh
{
	std::shared_ptr<BaseMesh> mesh = nullptr;
	/*!
	 * Stores the color of each mesh buffer.
	 */
	std::vector<ItemPartColor> bufferColors;
	/*!
	 * If false, all faces of the item should have the same brightness.
	 * Disables shading based on normal vectors.
	 */
	bool needsShading = true;

	ItemMesh() = default;
};


/*
	Wield item scene node, renders the wield mesh of some item
*/
class WieldMeshNode : public Node
{
public:
	WieldMeshNode(const ActorId actorId, 
		bool lighting, VisualEnvironment* env, PVWUpdater* updater);

	//destructor
	virtual ~WieldMeshNode();

	//! Renders event
	virtual bool PreRender(Scene* pScene);
	virtual bool Render(Scene* pScene);

	void CreateMesh();
	void SetMesh(const std::shared_ptr<BaseMesh>& mesh);
	void SetCube(const ContentFeatures& f, Vector3<float> wieldScale);
	void SetExtruded(const std::string& imageName, const std::string& overlayImage,
			Vector3<float> wieldScale, BaseTextureSource* texSrc, uint16_t numFrames);
	void SetItem(const ItemStack& item, bool checkWieldImage = true);

	// Sets the vertex color of the wield mesh.
	// Must only be used if the constructor was called with lighting = false
	void SetColor(SColor color);

	void SetNodeLightColor(SColor color);

	virtual BoundingBox<float>& GetBoundingBox();

	//! Removes a child from this scene node.
	//! Implemented here, to be able to remove the shadow properly, if there is one,
	//! or to remove attached childs.
	virtual int DetachChild(std::shared_ptr<Node> const& child);

	//! Returns the current mesh
	const std::shared_ptr<BaseMesh>& GetMesh(void) { return mMesh; }

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
	/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
	\param texture New texture to be used. */
	virtual void SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture);

	//! Sets the material type of all materials in this scene node to a new material type.
	/** \param newType New type of material to be set. */
	virtual void SetMaterialType(MaterialType newType);

	//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
	/** In this way it is possible to change the materials a mesh
	causing all mesh scene nodes referencing this mesh to change too. */
	void SetReadOnlyMaterials(bool readonly);

	//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
	bool IsReadOnlyMaterials() const;

private:

	void UpdateShaderConstants(unsigned int idx, Scene* pScene);

	void ChangeToMesh(std::shared_ptr<BaseMesh> mesh);

	VisualEnvironment* mEnvironment;

	std::vector<std::shared_ptr<BlendState>> mBlendStates;
	std::vector<std::shared_ptr<DepthStencilState>> mDepthStencilStates;
	std::shared_ptr<RasterizerState> mRasterizerState;

	std::vector<std::shared_ptr<Visual>> mVisuals;
	std::shared_ptr<BaseMesh> mMesh;
	std::shared_ptr<ShadowVolumeNode> mShadow;

	MaterialType mMaterialType;

	// True if EMF_LIGHTING should be enabled.
	bool mLighting;

	bool mEnableShaders;
	bool mAnisotropicFilter;
	bool mBilinearFilter;
	bool mTrilinearFilter;
	/*!
	 * Stores the colors of the mesh's mesh buffers.
	 * This does not include lighting.
	 */
	std::vector<ItemPartColor> mColors;
	/*!
	 * The base color of this mesh. This is the default
	 * for all mesh buffers.
	 */
	SColor mBaseColor;

	// Bounding box culling is disabled for this type of scene node,
	// so this variable is just required so we can implement
	// GetBoundingBox() and is set to an empty box.
	BoundingBox<float> mBoundingBox;

	int mPassCount;
	bool mReadOnlyMaterials;
};

void GetItemMesh(const ItemStack& item, ItemMesh* result, VisualEnvironment* env);

std::shared_ptr<BaseMesh> GetExtrudedMesh(BaseTextureSource* texSrc,
    const std::string& imageName, const std::string& overlayName);

/*!
 * Applies overlays, textures and optionally materials to the given mesh and
 * extracts tile colors for colorization.
 * \param matType overrides the buffer's material type, but can also
 * be NULL to leave the original material.
 * \param colors returns the colors of the mesh buffers in the mesh.
 */
void PostProcessNodeMesh(std::shared_ptr<BaseMesh> mesh, 
    const ContentFeatures& f, bool useShaders, bool setMaterial, 
    const MaterialType* matType, std::vector<ItemPartColor>* colors, 
    bool applyScale = false);

#endif