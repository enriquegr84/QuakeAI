// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "GraphNode.h"

#include "../Quake.h"

#include "Core/OS/OS.h"

#include "AI/AIManager.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

#include "Graphic/Scene/Element/CameraNode.h"
#include "Graphic/Scene/Scene.h"

//! constructor
GraphNode::GraphNode(const ActorId actorId, PVWUpdater* updater, 
	const std::shared_ptr<Texture2>& texture, Vector3<float> size,
	const std::shared_ptr<PathingGraph>& pathingGraph) : 
	Node(actorId, NT_MESH), mTexture(texture), mSize(size), mPassCount(0)
{
	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();

	const ClusterMap& clusters = pathingGraph->GetClusters();
	for (ClusterMap::const_iterator it = clusters.begin(); it != clusters.end(); ++it)
	{
		Cluster* cluster = (*it).second;

		uint32_t r = Randomizer::Rand() % 256;
		uint32_t g = Randomizer::Rand() % 256;
		uint32_t b = Randomizer::Rand() % 256;
		SColorF clr = SColor(40, r, g, b);
		mColors[cluster->GetId()] = clr.ToArray();
	}
}

GraphNode::~GraphNode()
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
}

void GraphNode::GenerateMesh(
	const std::unordered_map<PathingNode*, float>& selectedNodes,
	const std::shared_ptr<PathingGraph>& pathingGraph)
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
	mVisuals.clear();
	mBlendStates.clear();
	mDepthStencilStates.clear();

	VertexFormat vformat;
	vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
	vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
	vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

	PathingNodeVec transparentNodes, solidNodes;
	const PathingNodeMap& pathingNodes = pathingGraph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;

		if (selectedNodes.find(node) != selectedNodes.end())
			solidNodes.push_back(node);
		else
			transparentNodes.push_back(node);
	}

	mMesh = std::make_shared<NormalMesh>();
	if (!transparentNodes.empty())
	{
		BaseMeshBuffer* meshBuffer = new MeshBuffer(vformat, 
			24 * (unsigned int)transparentNodes.size(), 
			12 * (unsigned int)transparentNodes.size(), sizeof(unsigned int));
		mMesh->AddMeshBuffer(meshBuffer);

		std::shared_ptr<Material> material = std::make_shared<Material>();
		material->mType = MaterialType::MT_TRANSPARENT;
		material->mBlendTarget.enable = true;
		material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
		material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
		material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
		material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

		material->mDepthBuffer = true;
		material->mDepthMask = DepthStencilState::MASK_ALL;

		material->mFillMode = RasterizerState::FILL_SOLID;
		material->mCullMode = RasterizerState::CULL_NONE;
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		{
			meshBuffer->GetMaterial() = material;
			meshBuffer->GetMaterial()->SetTexture(0, mTexture);
		}
		GenerateGeometry(meshBuffer, transparentNodes);
	}

	if (!solidNodes.empty())
	{
		BaseMeshBuffer* meshBuffer = new MeshBuffer(vformat, 
			24 * (unsigned int)solidNodes.size(), 
			12 * (unsigned int)solidNodes.size(), sizeof(unsigned int));
		mMesh->AddMeshBuffer(meshBuffer);

		std::shared_ptr<Material> material = std::make_shared<Material>();
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		{
			meshBuffer->GetMaterial() = material;
			meshBuffer->GetMaterial()->SetTexture(0, mTexture);
		}
		GenerateGeometry(meshBuffer, solidNodes);
	}

	for (unsigned int mb = 0; mb < mMesh->GetMeshBufferCount(); mb++)
	{
		std::shared_ptr<BaseMeshBuffer> meshBuffer = mMesh->GetMeshBuffer(mb);

		mBlendStates.push_back(std::make_shared<BlendState>());
		mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
			meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::WRAP, SamplerState::WRAP);
		std::shared_ptr<Visual> visual = std::make_shared<Visual>(
			meshBuffer->GetVertice(), meshBuffer->GetIndice(), effect);
		visual->UpdateModelBound();
		mVisuals.push_back(visual);
		mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
	}
}

void GraphNode::GenerateMesh(
	const std::map<unsigned short, unsigned short>& selectedClusters,
	const std::shared_ptr<PathingGraph>& pathingGraph)
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
	mVisuals.clear();
	mBlendStates.clear();
	mDepthStencilStates.clear();

	VertexFormat vformat;
	vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
	vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
	vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

	PathingNodeVec transparentNodes, solidNodes;
	const PathingNodeMap& pathingNodes = pathingGraph->GetNodes();
	for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
	{
		PathingNode* node = (*it).second;

		if (selectedClusters.find(node->GetCluster()) == selectedClusters.end())
			solidNodes.push_back(node);
		else
			transparentNodes.push_back(node);
	}
	/*
	const ClusterMap& pathingClusters = pathingGraph->GetClusters();
	for (ClusterMap::const_iterator it = pathingClusters.begin(); it != pathingClusters.end(); ++it)
	{
		Cluster* cluster = (*it).second;
		const PathingNodeMap& pathingNodes = cluster->GetNodes();
		for (PathingNodeMap::const_iterator it = pathingNodes.begin(); it != pathingNodes.end(); ++it)
		{
			PathingNode* node = (*it).second;

			if (node == cluster->GetNode())
				solidNodes.push_back(node);
			else
				transparentNodes.push_back(node);
		}
	}
	*/
	mMesh = std::make_shared<NormalMesh>();
	if (!transparentNodes.empty())
	{
		BaseMeshBuffer* meshBuffer = new MeshBuffer(vformat, 
			24 * (unsigned int)transparentNodes.size(), 
			12 * (unsigned int)transparentNodes.size(), sizeof(unsigned int));
		mMesh->AddMeshBuffer(meshBuffer);

		std::shared_ptr<Material> material = std::make_shared<Material>();
		material->mType = MaterialType::MT_TRANSPARENT;
		material->mBlendTarget.enable = true;
		material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
		material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
		material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
		material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

		material->mDepthBuffer = true;
		material->mDepthMask = DepthStencilState::MASK_ALL;

		material->mFillMode = RasterizerState::FILL_SOLID;
		material->mCullMode = RasterizerState::CULL_NONE;
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		{
			meshBuffer->GetMaterial() = material;
			meshBuffer->GetMaterial()->SetTexture(0, mTexture);
		}
		GenerateGeometry(meshBuffer, transparentNodes);
	}

	if (!solidNodes.empty())
	{
		BaseMeshBuffer* meshBuffer = new MeshBuffer(vformat, 
			24 * (unsigned int)solidNodes.size(), 
			12 * (unsigned int)solidNodes.size(), sizeof(unsigned int));
		mMesh->AddMeshBuffer(meshBuffer);

		std::shared_ptr<Material> material = std::make_shared<Material>();
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		{
			meshBuffer->GetMaterial() = material;
			meshBuffer->GetMaterial()->SetTexture(0, mTexture);
		}
		GenerateGeometry(meshBuffer, solidNodes);
	}

	for (unsigned int mb = 0; mb < mMesh->GetMeshBufferCount(); mb++)
	{
		std::shared_ptr<BaseMeshBuffer> meshBuffer = mMesh->GetMeshBuffer(mb);

		mBlendStates.push_back(std::make_shared<BlendState>());
		mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ColorEffectVS.glsl");
		path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ColorEffectVS.hlsl");
		path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
			meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE),
			SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::WRAP, SamplerState::WRAP);
		std::shared_ptr<Visual> visual = std::make_shared<Visual>(
			meshBuffer->GetVertice(), meshBuffer->GetIndice(), effect);
		visual->UpdateModelBound();
		mVisuals.push_back(visual);
		mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
	}
}

void GraphNode::GenerateGeometry(BaseMeshBuffer* meshBuffer, const PathingNodeVec& nodes)
{
	LogAssert(!nodes.empty(), "Nodes can't be empty");

	// Generate geometry.
	struct Vertex
	{
		Vector3<float> position;
		Vector2<float> tcoord;
		Vector4<float> color;
	};
	unsigned int numVertices = meshBuffer->GetVertice()->GetNumElements();
	Vertex* vertex = meshBuffer->GetVertice()->Get<Vertex>();

	Vector3<float> pos;
	Vector2<float> tcd;

	// Choose vertex normals in the directions.
	float normal = 1.0f;

	unsigned int v = 0;
	for (unsigned int z = 0; z < 2; ++z)
	{
		float fz = static_cast<float>(z), omfz = 1.0f - fz;
		float zSign = 2.0f * fz - 1.0f;
		pos[2] = zSign * mSize[2];
		for (unsigned int y = 0; y < 2; ++y)
		{
			float fy = static_cast<float>(y);
			float ySign = 2.0f * fy - 1.0f;
			pos[1] = ySign * mSize[1];
			tcd[1] = (1.0f - fy) * omfz + (1.0f - fy) * fz;
			for (unsigned int x = 0; x < 2; ++x, ++v)
			{
				float fx = static_cast<float>(x);
				float xSign = 2.0f * fx - 1.0f;
				pos[0] = xSign * mSize[0];
				tcd[0] = fx * omfz + fx * fz;

				vertex[v].position = pos;
				vertex[v].tcoord = tcd;
			}
		}
	}

	for (unsigned int y = 0; y < 2; ++y)
	{
		float fy = static_cast<float>(y), omfy = 1.0f - fy;
		float ySign = 2.0f * fy - 1.0f;
		pos[1] = ySign * mSize[1];
		for (unsigned int z = 0; z < 2; ++z)
		{
			float fz = static_cast<float>(z);
			float zSign = 2.0f * fz - 1.0f;
			pos[2] = zSign * mSize[2];
			tcd[1] = (1.0f - fz) * omfy + (1.0f - fz) * fy;
			for (unsigned int x = 0; x < 2; ++x, ++v)
			{
				float fx = static_cast<float>(x);
				float xSign = 2.0f * fx - 1.0f;
				pos[0] = xSign * mSize[0];
				tcd[0] = fx * omfy + fx * fy;

				vertex[v].position = pos;
				vertex[v].tcoord = tcd;
			}
		}
	}

	for (unsigned int x = 0; x < 2; ++x)
	{
		float fx = static_cast<float>(x), omfx = 1.0f - fx;
		float xSign = 2.0f * fx - 1.0f;
		pos[0] = xSign * mSize[0];
		for (unsigned int z = 0; z < 2; ++z)
		{
			float fz = static_cast<float>(z);
			float zSign = 2.0f * fz - 1.0f;
			pos[2] = zSign * mSize[2];
			tcd[1] = (1.0f - fz) * omfx + (1.0f - fz) * fx;
			for (unsigned int y = 0; y < 2; ++y, ++v)
			{
				float fy = static_cast<float>(y);
				float ySign = 2.0f * fy - 1.0f;
				pos[1] = ySign * mSize[1];
				tcd[0] = fy * omfx + fy * fx;

				vertex[v].position = pos;
				vertex[v].tcoord = tcd;
			}
		}
	}

	for (unsigned int n = 1; n < nodes.size(); n++)
	{
		for (unsigned int vtx = 0; vtx < 24; vtx++, v++)
		{
			vertex[v].position = vertex[vtx].position + nodes[n]->GetPosition();
			vertex[v].color = mColors[nodes[n]->GetCluster()];
			vertex[v].tcoord = vertex[vtx].tcoord;
		}
	}
	for (unsigned int vtx = 0; vtx < 24; vtx++, v++)
	{
		vertex[vtx].position += nodes[0]->GetPosition();
		vertex[vtx].color = mColors[nodes[0]->GetCluster()];
	}

	// fill indices
	unsigned int tr = 0;
	for (unsigned int v = 0; tr < meshBuffer->GetIndice()->GetNumPrimitives(); v += 4)
	{
		meshBuffer->GetIndice()->SetTriangle(tr++, 0 + v, 2 + v, 3 + v);
		meshBuffer->GetIndice()->SetTriangle(tr++, 0 + v, 3 + v, 1 + v);
	}
}

//! pre render method
bool GraphNode::PreRender(Scene* pScene)
{
	if (IsVisible())
	{
		// because this node supports rendering of mixed mode meshes consisting of
		// transparent and solid material at the same time, we need to go through all
		// materials, check of what type they are and register this node for the right
		// render pass according to that.

		mPassCount = 0;
		int transparentCount = 0;
		int solidCount = 0;

		// count transparent and solid materials in this scene node
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		{
			if (GetMaterial(i)->IsTransparent())
				++transparentCount;
			else
				++solidCount;

			if (solidCount && transparentCount)
				break;
		}

		// register according to material types counted
		if (!pScene->IsCulled(this))
		{
			if (solidCount)
				pScene->AddToRenderQueue(RP_SOLID, shared_from_this());

			if (transparentCount)
				pScene->AddToRenderQueue(RP_TRANSPARENT, shared_from_this());
		}
	}

	return Node::PreRender(pScene);
}

//! renders the node.
bool GraphNode::Render(Scene* pScene)
{
	if (!mMesh || !Renderer::Get())
		return false;

	bool isTransparentPass =
		pScene->GetCurrentRenderPass() == RP_TRANSPARENT;
	++mPassCount;

	for (unsigned int i = 0; i < GetVisualCount(); ++i)
	{
		// only render transparent buffer if this is the transparent render pass
		// and solid only in solid pass
		bool transparent = (mMesh->GetMeshBuffer(i)->GetMaterial()->IsTransparent());
		if (transparent == isTransparentPass)
		{
			if (mMesh->GetMeshBuffer(i)->GetMaterial()->Update(mBlendStates[i]))
				Renderer::Get()->Unbind(mBlendStates[i]);
			if (mMesh->GetMeshBuffer(i)->GetMaterial()->Update(mDepthStencilStates[i]))
				Renderer::Get()->Unbind(mDepthStencilStates[i]);
			if (mMesh->GetMeshBuffer(i)->GetMaterial()->Update(mRasterizerState))
				Renderer::Get()->Unbind(mRasterizerState);

			Renderer::Get()->SetBlendState(mBlendStates[i]);
			Renderer::Get()->SetDepthStencilState(mDepthStencilStates[i]);
			Renderer::Get()->SetRasterizerState(mRasterizerState);

			Renderer::Get()->Draw(mVisuals[i]);

			Renderer::Get()->SetDefaultBlendState();
			Renderer::Get()->SetDefaultDepthStencilState();
			Renderer::Get()->SetDefaultRasterizerState();
		}
	}

	return true;
}

//! returns the axis aligned bounding box of this node
BoundingBox<float>& GraphNode::GetBoundingBox()
{
	return mMesh ? mMesh->GetBoundingBox() : BoundingBox<float>();
}

//! Returns the visual based on the zero based index i. To get the amount 
//! of visuals used by this scene node, use GetVisualCount(). 
//! This function is needed for inserting the node into the scene hierarchy 
//! at an optimal position for minimizing renderstate changes, but can also 
//! be used to directly modify the visual of a scene node.
std::shared_ptr<Visual> const& GraphNode::GetVisual(unsigned int i)
{
	if (i >= mVisuals.size())
		return nullptr;

	return mVisuals[i];
}

//! return amount of visuals of this scene node.
size_t GraphNode::GetVisualCount() const
{
	return mVisuals.size();
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& GraphNode::GetMaterial(unsigned int i)
{
	if (mMesh->GetMeshBuffer(i))
		return mMesh->GetMeshBuffer(i)->GetMaterial();

	return nullptr;
}

//! returns amount of materials used by this scene node.
size_t GraphNode::GetMaterialCount() const
{
	return mMesh->GetMeshBufferCount();
}

//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
\param texture New texture to be used. */
void GraphNode::SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture)
{
	if (textureLayer >= MATERIAL_MAX_TEXTURES)
		return;

	for (unsigned int i = 0; i<GetMaterialCount(); ++i)
		GetMaterial(i)->SetTexture(textureLayer, texture);

	for (unsigned int i = 0; i < GetVisualCount(); ++i)
	{
		std::shared_ptr<Visual> visual = GetVisual(i);
		if (visual)
		{
			std::shared_ptr<Texture2Effect> textureEffect =
				std::dynamic_pointer_cast<Texture2Effect>(visual->GetEffect());
			if (textureEffect)
				textureEffect->SetTexture(texture);
		}
	}
}

//! Sets the material type of all materials in this scene node to a new material type.
/** \param newType New type of material to be set. */
void GraphNode::SetMaterialType(MaterialType newType)
{
	for (unsigned int i = 0; i<GetMaterialCount(); ++i)
		GetMaterial(i)->mType = newType;
}