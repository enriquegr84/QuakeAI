// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "MeshNode.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

#include "Graphic/Scene/Scene.h"


//! constructor
StaticMeshNode::StaticMeshNode(const ActorId actorId, PVWUpdater* updater, const std::shared_ptr<BaseMesh>& mesh)
:	Node(actorId, NT_STATIC_MESH), mMesh(0), mShadow(0), mPassCount(0)
{
	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();

	SetMesh(mesh);
}


//! destructor
StaticMeshNode::~StaticMeshNode()
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
}


//! Sets a new mesh
void StaticMeshNode::SetMesh(const std::shared_ptr<BaseMesh>& mesh)
{
	if (!mesh)
		return; // won't set null mesh

	mMesh = mesh;

	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
	mVisuals.clear();
	std::vector<std::shared_ptr<BaseMeshBuffer>> meshBuffers;
	unsigned int meshBufferVerticeCount = 0, meshBufferPrimitiveCount = 0;

	std::map<std::string, std::vector<std::shared_ptr<BaseMeshBuffer>>>
		meshBufferDiffuse, meshBufferTransparentDiffuse;
	std::map<std::string, unsigned int> meshBufferVertices, meshBufferTransparentVertices;
	std::map<std::string, unsigned int> meshBufferPrimitives, meshBufferTransparentPrimitives;
	for (unsigned int i = 0; i < mMesh->GetMeshBufferCount(); ++i)
	{
		const std::shared_ptr<BaseMeshBuffer>& meshBuffer = mMesh->GetMeshBuffer(i);
		if (meshBuffer)
		{
			std::shared_ptr<Texture2> textureDiffuse =
				meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE);
			if (textureDiffuse)
			{
				std::string tex =
					std::to_string(textureDiffuse->GetWidth()) + " " +
					std::to_string(textureDiffuse->GetHeight());
				if (meshBuffer->GetMaterial()->IsTransparent())
				{
					meshBufferTransparentDiffuse[tex].push_back(meshBuffer);

					if (meshBufferTransparentVertices.find(tex) == meshBufferTransparentVertices.end())
					{
						meshBufferTransparentVertices[tex] = 0;
						meshBufferTransparentPrimitives[tex] = 0;
					}
					meshBufferTransparentVertices[tex] += meshBuffer->GetVertice()->GetNumElements();
					meshBufferTransparentPrimitives[tex] += meshBuffer->GetIndice()->GetNumPrimitives();
				}
				else
				{
					meshBufferDiffuse[tex].push_back(meshBuffer);

					if (meshBufferVertices.find(tex) == meshBufferVertices.end())
					{
						meshBufferVertices[tex] = 0;
						meshBufferPrimitives[tex] = 0;
					}
					meshBufferVertices[tex] += meshBuffer->GetVertice()->GetNumElements();
					meshBufferPrimitives[tex] += meshBuffer->GetIndice()->GetNumPrimitives();
				}
			}
			else
			{
				meshBufferVerticeCount += meshBuffer->GetVertice()->GetNumElements();
				meshBufferPrimitiveCount += meshBuffer->GetIndice()->GetNumPrimitives();
				meshBuffers.push_back(meshBuffer);
			}
		}
	}

	std::map<std::string, std::vector<std::shared_ptr<BaseMeshBuffer>>>::const_iterator it;
	for (it = meshBufferDiffuse.begin(); it != meshBufferDiffuse.end(); it++)
	{
		struct Vertex
		{
			Vector3<float> position;
			Vector3<float> texCoord;
		};
		VertexFormat vertexFormat;
		vertexFormat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vertexFormat.Bind(VA_TEXCOORD, DF_R32G32B32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vBuffer = std::make_shared<VertexBuffer>(
			vertexFormat, meshBufferVertices[it->first]);
		Vertex* vertex = vBuffer->Get<Vertex>();

		std::shared_ptr<IndexBuffer> iBuffer = std::make_shared<IndexBuffer>(
			IP_TRIMESH, meshBufferPrimitives[it->first], sizeof(unsigned int));

		size_t split = it->first.find(" ");
		std::shared_ptr<Texture2Array> textureArray =
			std::make_shared<Texture2Array>((int)it->second.size(), DF_R8G8B8A8_UNORM,
				atoi(it->first.substr(0, split).c_str()), atoi(it->first.substr(split + 1).c_str()), false);
		//textureArray->AutogenerateMipmaps();
		unsigned char* textureData = textureArray->Get<unsigned char>();

		std::shared_ptr<Material> material;
		SamplerState::Filter samplerfilter;
		SamplerState::Mode samplerModeU, samplerModeV;
		unsigned int bufferCount = 0, vertexCount = 0, idx = 0;
		for (unsigned int mb = 0; mb < it->second.size(); mb++)
		{
			const std::shared_ptr<BaseMeshBuffer>& meshBuffer = it->second[mb];
			if (meshBuffer)
			{
				std::shared_ptr<Texture2> textureDiffuse =
					meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE);
				samplerfilter = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mFilter;
				samplerModeU = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeU;
				samplerModeV = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeV;
				material = meshBuffer->GetMaterial();

				// fill vertices
				for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
				{
					vertex[vertexCount + i].position = meshBuffer->Position(i);
					vertex[vertexCount + i].texCoord = HLift(meshBuffer->TCoord(0, i), (float)bufferCount);
				}

				//fill textures
				std::memcpy(textureData, textureDiffuse->GetData(), textureDiffuse->GetNumBytes());
				textureData += textureDiffuse->GetNumBytes();

				//fill indices
				unsigned int* index = meshBuffer->GetIndice()->Get<unsigned int>();
				for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i++)
				{
					iBuffer->SetTriangle(idx++,
						vertexCount + index[i * 3],
						vertexCount + index[i * 3 + 1],
						vertexCount + index[i * 3 + 2]);
				}

				vertexCount += meshBuffer->GetVertice()->GetNumElements();
				bufferCount++;
			}
		}

		mMaterials.push_back(material);
		mBlendStates.push_back(std::make_shared<BlendState>());
		mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ArrayEffectVS.glsl");
		path.push_back("Effects/Texture2ArrayEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ArrayEffectVS.hlsl");
		path.push_back("Effects/Texture2ArrayEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<Texture2ArrayEffect> effect = std::make_shared<Texture2ArrayEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), 
			textureArray, samplerfilter, samplerModeU, samplerModeV);

		std::shared_ptr<Visual> visual = std::make_shared<Visual>(vBuffer, iBuffer, effect);
		visual->UpdateModelBound();
		mVisuals.push_back(visual);
		mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
	}

	for (it = meshBufferTransparentDiffuse.begin(); it != meshBufferTransparentDiffuse.end(); it++)
	{
		struct Vertex
		{
			Vector3<float> position;
			Vector3<float> texCoord;
		};
		VertexFormat vertexFormat;
		vertexFormat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vertexFormat.Bind(VA_TEXCOORD, DF_R32G32B32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vBuffer = std::make_shared<VertexBuffer>(
			vertexFormat, meshBufferTransparentVertices[it->first]);
		Vertex* vertex = vBuffer->Get<Vertex>();

		std::shared_ptr<IndexBuffer> iBuffer = std::make_shared<IndexBuffer>(
			IP_TRIMESH, meshBufferTransparentPrimitives[it->first], sizeof(unsigned int));

		size_t split = it->first.find(" ");
		std::shared_ptr<Texture2Array> textureArray =
			std::make_shared<Texture2Array>((int)it->second.size(), DF_R8G8B8A8_UNORM,
				atoi(it->first.substr(0, split).c_str()), atoi(it->first.substr(split + 1).c_str()), false);
		//textureArray->AutogenerateMipmaps();
		unsigned char* textureData = textureArray->Get<unsigned char>();

		std::shared_ptr<Material> material;
		SamplerState::Filter samplerfilter;
		SamplerState::Mode samplerModeU, samplerModeV;
		unsigned int bufferCount = 0, vertexCount = 0, idx = 0;
		for (unsigned int mb = 0; mb < it->second.size(); mb++)
		{
			const std::shared_ptr<BaseMeshBuffer>& meshBuffer = it->second[mb];
			if (meshBuffer)
			{
				std::shared_ptr<Texture2> textureDiffuse =
					meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE);
				samplerfilter = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mFilter;
				samplerModeU = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeU;
				samplerModeV = meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeV;
				material = meshBuffer->GetMaterial();

				// fill vertices
				for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
				{
					vertex[vertexCount + i].position = meshBuffer->Position(i);
					vertex[vertexCount + i].texCoord = HLift(meshBuffer->TCoord(0, i), (float)bufferCount);
				}

				//fill textures
				std::memcpy(textureData, textureDiffuse->GetData(), textureDiffuse->GetNumBytes());
				textureData += textureDiffuse->GetNumBytes();

				//fill indices
				unsigned int* index = meshBuffer->GetIndice()->Get<unsigned int>();
				for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i++)
				{
					iBuffer->SetTriangle(idx++,
						vertexCount + index[i * 3],
						vertexCount + index[i * 3 + 1],
						vertexCount + index[i * 3 + 2]);
				}

				vertexCount += meshBuffer->GetVertice()->GetNumElements();
				bufferCount++;
			}
		}

		mMaterials.push_back(material);
		mBlendStates.push_back(std::make_shared<BlendState>());
		mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/Texture2ArrayEffectVS.glsl");
		path.push_back("Effects/Texture2ArrayEffectPS.glsl");
#else
		path.push_back("Effects/Texture2ArrayEffectVS.hlsl");
		path.push_back("Effects/Texture2ArrayEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<Texture2ArrayEffect> effect = std::make_shared<Texture2ArrayEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), 
			textureArray, samplerfilter, samplerModeU, samplerModeV);

		std::shared_ptr<Visual> visual = std::make_shared<Visual>(vBuffer, iBuffer, effect);
		visual->UpdateModelBound();
		mVisuals.push_back(visual);
		mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
	}

	if (meshBuffers.size() > 0)
	{
		struct Vertex
		{
			Vector3<float> position;
			Vector4<float> color;
		};
		VertexFormat vertexFormat;
		vertexFormat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
		vertexFormat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

		std::shared_ptr<VertexBuffer> vBuffer = 
			std::make_shared<VertexBuffer>(vertexFormat, meshBufferVerticeCount);
		Vertex* vertex = vBuffer->Get<Vertex>();

		std::shared_ptr<IndexBuffer> iBuffer = std::make_shared<IndexBuffer>(
			IP_TRIMESH, meshBufferPrimitiveCount, sizeof(unsigned int));

		std::shared_ptr<Material> material;
		unsigned int vertexCount = 0, idx = 0;
		for (unsigned int mb = 0; mb < meshBuffers.size(); mb++)
		{
			const std::shared_ptr<BaseMeshBuffer>& meshBuffer = meshBuffers[mb];
			if (meshBuffer)
			{
				material = meshBuffer->GetMaterial();

				// fill vertices
				for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
					vertex[vertexCount + i].position = meshBuffer->Position(i);

				//fill indices
				unsigned int* index = meshBuffer->GetIndice()->Get<unsigned int>();
				for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i++)
				{
					iBuffer->SetTriangle(idx++,
						vertexCount + index[i * 3],
						vertexCount + index[i * 3 + 1],
						vertexCount + index[i * 3 + 2]);
				}

				vertexCount += meshBuffer->GetVertice()->GetNumElements();
			}
		}

		mMaterials.push_back(material);
		mBlendStates.push_back(std::make_shared<BlendState>());
		mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

		std::vector<std::string> path;
#if defined(_OPENGL_)
		path.push_back("Effects/ConstantColorEffectVS.glsl");
		path.push_back("Effects/ConstantColorEffectPS.glsl");
#else
		path.push_back("Effects/ConstantColorEffectVS.hlsl");
		path.push_back("Effects/ConstantColorEffectPS.hlsl");
#endif
		std::shared_ptr<ResHandle> resHandle =
			ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

		const std::shared_ptr<ShaderResourceExtraData>& extra =
			std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
		if (!extra->GetProgram())
			extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

		std::shared_ptr<ConstantColorEffect> effect = std::make_shared<ConstantColorEffect>(
			ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), Vector4<float>::Zero());

		std::shared_ptr<Visual> visual = std::make_shared<Visual>(vBuffer, iBuffer, effect);
		visual->UpdateModelBound();
		mVisuals.push_back(visual);
		mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
	}
}


//! frame
bool StaticMeshNode::PreRender(Scene *pScene)
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
bool StaticMeshNode::Render(Scene *pScene)
{
	if (!mMesh || !Renderer::Get())
		return false;

	bool isTransparentPass = 
		pScene->GetCurrentRenderPass() == RP_TRANSPARENT;
	++mPassCount;

	if (mShadow && mPassCount==1)
		mShadow->UpdateShadowVolumes(pScene);

	for (unsigned int i = 0; i < GetVisualCount(); ++i)
	{
		// only render transparent buffer if this is the transparent render pass
		// and solid only in solid pass
		bool transparent = (mMaterials[i]->IsTransparent());
		if (transparent == isTransparentPass)
		{
			if (mMaterials[i]->Update(mBlendStates[i]))
				Renderer::Get()->Unbind(mBlendStates[i]);
			if (mMaterials[i]->Update(mDepthStencilStates[i]))
				Renderer::Get()->Unbind(mDepthStencilStates[i]);
			if (mMaterials[i]->Update(mRasterizerState))
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
BoundingBox<float>& StaticMeshNode::GetBoundingBox()
{
	return mMesh ? mMesh->GetBoundingBox() : mBoundingBox;
}


//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
int StaticMeshNode::DetachChild(std::shared_ptr<Node> const& child)
{
	if (child && mShadow == child)
		mShadow = 0;

	if (Node::DetachChild(child))
		return true;

	return false;
}


//! Creates shadow volume scene node as child of this node
//! and returns a pointer to it.
std::shared_ptr<ShadowVolumeNode> StaticMeshNode::AddShadowVolumeNode(const ActorId actorId,
	Scene* pScene, const std::shared_ptr<BaseMesh>& shadowMesh, bool zfailmethod, float infinity)
{
	/*
	if (!Renderer::Get()->QueryFeature(VDF_STENCIL_BUFFER))
		return nullptr;
	*/
	mShadow = std::shared_ptr<ShadowVolumeNode>(
		new ShadowVolumeNode(actorId, mPVWUpdater, shadowMesh ? shadowMesh : mMesh, zfailmethod, infinity));
	shared_from_this()->AttachChild(mShadow);

	return mShadow;
}

//! Returns the visual based on the zero based index i. To get the amount 
//! of visuals used by this scene node, use GetVisualCount(). 
//! This function is needed for inserting the node into the scene hierarchy 
//! at an optimal position for minimizing renderstate changes, but can also 
//! be used to directly modify the visual of a scene node.
std::shared_ptr<Visual> const& StaticMeshNode::GetVisual(unsigned int i)
{
	if (i >= mVisuals.size())
		return nullptr;

	return mVisuals[i];
}

//! return amount of visuals of this scene node.
size_t StaticMeshNode::GetVisualCount() const
{
	return mVisuals.size();
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& StaticMeshNode::GetMaterial(unsigned int i)
{
	if (i >= mMaterials.size())
		return nullptr;

	return mMaterials[i];
}

//! returns amount of materials used by this scene node.
size_t StaticMeshNode::GetMaterialCount() const
{
	return mMaterials.size();
}

//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
\param texture New texture to be used. */
void StaticMeshNode::SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture)
{
	if (textureLayer >= MATERIAL_MAX_TEXTURES)
		return;

	for (unsigned int i = 0; i<GetMaterialCount(); ++i)
		GetMaterial(i)->SetTexture(textureLayer, texture);
}

//! Sets the material type of all materials in this scene node to a new material type.
/** \param newType New type of material to be set. */
void StaticMeshNode::SetMaterialType(MaterialType newType)
{
	for (unsigned int i = 0; i<GetMaterialCount(); ++i)
		GetMaterial(i)->mType = newType;
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void StaticMeshNode::SetReadOnlyMaterials(bool readonly)
{
	mReadOnlyMaterials = readonly;
}


//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool StaticMeshNode::IsReadOnlyMaterials() const
{
	return mReadOnlyMaterials;
}