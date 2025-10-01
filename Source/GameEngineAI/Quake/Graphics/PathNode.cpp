// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "PathNode.h"

#include "../Quake.h"

#include "Core/OS/OS.h"

#include "AI/AIManager.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

#include "Graphic/Scene/Element/CameraNode.h"
#include "Graphic/Scene/Scene.h"

//! constructor
PathNode::PathNode(const ActorId actorId, PVWUpdater* updater, 
	const std::shared_ptr<Texture2>& texture, Vector3<float> size) : 
	Node(actorId, NT_MESH), mTexture(texture), mSize(size)
{
	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();
	mBlendState = std::make_shared<BlendState>();
	mDepthStencilState = std::make_shared<DepthStencilState>();
}

PathNode::~PathNode()
{
	mPVWUpdater->Unsubscribe(mVisual->GetEffect()->GetPVWMatrixConstant());
}

void PathNode::GenerateMesh(const std::vector<Vector3<float>>& nodes)
{
	VertexFormat vformat;
	vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
	vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);

	mMesh = std::make_shared<NormalMesh>();
	if (!nodes.empty())
	{
		BaseMeshBuffer* meshBuffer = new MeshBuffer(vformat, 
			24 * (unsigned int)nodes.size(), 
			12 * (unsigned int)nodes.size(), sizeof(unsigned int));
		mMesh->AddMeshBuffer(meshBuffer);

		std::shared_ptr<Material> material = std::make_shared<Material>();
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		{
			meshBuffer->GetMaterial() = material;
			meshBuffer->GetMaterial()->SetTexture(0, mTexture);
		}
		GenerateGeometry(meshBuffer, nodes);
	}

	for (unsigned int mb = 0; mb < mMesh->GetMeshBufferCount(); mb++)
	{
		std::shared_ptr<BaseMeshBuffer> meshBuffer = mMesh->GetMeshBuffer(mb);

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2EffectVS.glsl");
		path.push_back("Effects/Texture2EffectPS.glsl");
#else
		path.push_back("Effects/Texture2EffectVS.hlsl");
		path.push_back("Effects/Texture2EffectPS.hlsl");
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
		mVisual = std::make_shared<Visual>(meshBuffer->GetVertice(), meshBuffer->GetIndice(), effect);
		mVisual->UpdateModelBound();

		mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
	}
}

void PathNode::GenerateGeometry(BaseMeshBuffer* meshBuffer, const std::vector<Vector3<float>>& nodes)
{
	LogAssert(!nodes.empty(), "Nodes can't be empty");

	// Generate geometry.
	struct Vertex
	{
		Vector3<float> position;
		Vector2<float> tcoord;
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
			vertex[v].position = vertex[vtx].position + nodes[n];
			vertex[v].tcoord = vertex[vtx].tcoord;
		}
	}
	for (unsigned int vtx = 0; vtx < 24; vtx++, v++)
	{
		vertex[vtx].position += nodes[0];
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
bool PathNode::PreRender(Scene* pScene)
{
	if (IsVisible())
	{
		// because this node supports rendering of mixed mode meshes consisting of
		// transparent and solid material at the same time, we need to go through all
		// materials, check of what type they are and register this node for the right
		// render pass according to that.

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
bool PathNode::Render(Scene* pScene)
{
	if (!mMesh || !Renderer::Get())
		return false;

	for (unsigned int i = 0; i < GetMaterialCount(); ++i)
	{
		if (GetMaterial(i)->Update(mBlendState))
			Renderer::Get()->Unbind(mBlendState);
		if (GetMaterial(i)->Update(mDepthStencilState))
			Renderer::Get()->Unbind(mDepthStencilState);
		if (GetMaterial(i)->Update(mRasterizerState))
			Renderer::Get()->Unbind(mRasterizerState);
	}

	Renderer::Get()->SetBlendState(mBlendState);
	Renderer::Get()->SetDepthStencilState(mDepthStencilState);
	Renderer::Get()->SetRasterizerState(mRasterizerState);

	Renderer* renderer = Renderer::Get();
	renderer->Update(mVisual->GetVertexBuffer());
	renderer->Draw(mVisual);

	Renderer::Get()->SetDefaultBlendState();
	Renderer::Get()->SetDefaultDepthStencilState();
	Renderer::Get()->SetDefaultRasterizerState();

	return true;
}

//! returns the axis aligned bounding box of this node
BoundingBox<float>& PathNode::GetBoundingBox()
{
	return mMesh ? mMesh->GetBoundingBox() : BoundingBox<float>();
}

//! Returns the visual based on the zero based index i. To get the amount 
//! of visuals used by this scene node, use GetVisualCount(). 
//! This function is needed for inserting the node into the scene hierarchy 
//! at an optimal position for minimizing renderstate changes, but can also 
//! be used to directly modify the visual of a scene node.
std::shared_ptr<Visual> const& PathNode::GetVisual(unsigned int i)
{
	return mVisual;
}

//! return amount of visuals of this scene node.
size_t PathNode::GetVisualCount() const
{
	return 1;
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& PathNode::GetMaterial(unsigned int i)
{
	if (mMesh->GetMeshBuffer(i))
		return mMesh->GetMeshBuffer(i)->GetMaterial();

	return nullptr;
}

//! returns amount of materials used by this scene node.
size_t PathNode::GetMaterialCount() const
{
	return mMesh->GetMeshBufferCount();
}

//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
\param texture New texture to be used. */
void PathNode::SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture)
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
void PathNode::SetMaterialType(MaterialType newType)
{
	for (unsigned int i = 0; i<GetMaterialCount(); ++i)
		GetMaterial(i)->mType = newType;
}