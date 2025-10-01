// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "ObjectMesh.h"

#include "Sky.h"

#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Scene.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

//! constructor
ObjectMeshNode::ObjectMeshNode(const ActorId actorId, unsigned int shaderId,
	VisualEnvironment* env, PVWUpdater* updater, const std::shared_ptr<BaseMesh>& mesh)
: Node(actorId, NT_MESH), mEnvironment(env), mShaderId(shaderId)
{
	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();

	SetMesh(mesh);
}

//! destructor
ObjectMeshNode::~ObjectMeshNode()
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
}

//! Sets a new mesh
void ObjectMeshNode::SetMesh(const std::shared_ptr<BaseMesh>& mesh)
{
	if (!mesh)
		return; // won't set null mesh

	mMesh = mesh;
	std::vector<std::shared_ptr<BaseMeshBuffer>> meshBuffers;
	for (unsigned int i = 0; i < mMesh->GetMeshBufferCount(); ++i)
		meshBuffers.push_back(mMesh->GetMeshBuffer(i));

	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
	mVisuals.clear();
	mBlendStates.clear();
	mDepthStencilStates.clear();
	for (unsigned int i = 0; i < meshBuffers.size(); ++i)
	{
		const std::shared_ptr<BaseMeshBuffer>& meshBuffer = meshBuffers[i];
		if (meshBuffer)
		{
			mBlendStates.push_back(std::make_shared<BlendState>());
			mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

			std::shared_ptr<Texture2> textureDiffuse = meshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE);
			if (!textureDiffuse)
			{
				std::shared_ptr<ResHandle>& resHandle =
					ResCache::Get()->GetHandle(&BaseResource(L"Art/UserControl/appbar.empty.png"));
				if (resHandle)
				{
					const std::shared_ptr<ImageResourceExtraData>& extra =
						std::static_pointer_cast<ImageResourceExtraData>(resHandle->GetExtra());
					textureDiffuse = extra->GetImage();
				}
			}

			ShaderInfo shader = mEnvironment->GetShaderSource()->GetShaderInfo(mShaderId);
			std::shared_ptr<ObjectEffect> effect = std::make_shared<ObjectEffect>(
				ProgramFactory::Get()->CreateFromProgram(shader.visualProgram), textureDiffuse,
				meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mFilter,
				meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeU,
				meshBuffer->GetMaterial()->mTextureLayer[TT_DIFFUSE].mModeV);

			std::shared_ptr<Visual> visual = std::make_shared<Visual>(
				meshBuffer->GetVertice(), meshBuffer->GetIndice(), effect);
			visual->UpdateModelBound();

			mVisuals.push_back(visual);
			mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
		}
	}
}

//! prerender
bool ObjectMeshNode::PreRender(Scene *pScene)
{
	return false;

	if (IsVisible())
	{
		// because this node supports rendering of mixed mode meshes consisting of
		// transparent and solid material at the same time, we need to go through all
		// materials, check of what type they are and register this node for the right
		// render pass according to that.
		int transparentCount = 0;
		int solidCount = 0;

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

//! render
bool ObjectMeshNode::Render(Scene *pScene)
{
	if (!mMesh || !Renderer::Get())
		return false;

	bool isTransparentPass =
		pScene->GetCurrentRenderPass() == RP_TRANSPARENT;
	++mPassCount;

	if (mShadow && mPassCount == 1)
		mShadow->UpdateShadowVolumes(pScene);

	for (unsigned int i = 0; i < GetVisualCount(); ++i)
	{
		// only render transparent buffer if this is the transparent render pass
		// and solid only in solid pass
		bool transparent = (GetMaterial(i)->IsTransparent());
		if (transparent == isTransparentPass)
		{
			if (GetMaterial(i)->Update(mBlendStates[i]))
				Renderer::Get()->Unbind(mBlendStates[i]);
			if (GetMaterial(i)->Update(mDepthStencilStates[i]))
				Renderer::Get()->Unbind(mDepthStencilStates[i]);
			if (GetMaterial(i)->Update(mRasterizerState))
				Renderer::Get()->Unbind(mRasterizerState);

			Renderer::Get()->SetBlendState(mBlendStates[i]);
			Renderer::Get()->SetDepthStencilState(mDepthStencilStates[i]);
			Renderer::Get()->SetRasterizerState(mRasterizerState);

			UpdateShaderConstants(i, pScene);

			Renderer::Get()->Update(mVisuals[i]->GetVertexBuffer());
			Renderer::Get()->Draw(mVisuals[i]);

			Renderer::Get()->SetDefaultBlendState();
			Renderer::Get()->SetDefaultDepthStencilState();
			Renderer::Get()->SetDefaultRasterizerState();
		}
	}

	return true;
}


void ObjectMeshNode::UpdateShaderConstants(unsigned int idx, Scene* pScene)
{
	std::shared_ptr<Visual> visual = GetVisual(idx);
	if (visual)
	{
		std::shared_ptr<ObjectEffect> effect =
			std::dynamic_pointer_cast<ObjectEffect>(visual->GetEffect());
		if (!effect)
			return;
		Matrix4x4<float> wMatrix = GetAbsoluteTransform().GetHMatrix();
		effect->SetWMatrix(wMatrix);
		Renderer::Get()->Update(effect->GetWMatrixConstant());

		Matrix4x4<float> vwMatrix = pScene->GetActiveCamera()->Get()->GetViewMatrix();
		vwMatrix = vwMatrix * wMatrix;
		effect->SetVWMatrix(vwMatrix);
		Renderer::Get()->Update(effect->GetVWMatrixConstant());

		Matrix4x4<float> pMatrix = pScene->GetActiveCamera()->Get()->GetProjectionMatrix();
		Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionViewMatrix();
		effect->SetPVMatrix(pvMatrix);
		Renderer::Get()->Update(effect->GetPVMatrixConstant());

		Matrix4x4<float> pvwMatrix = pvMatrix * wMatrix;
		effect->SetPVWMatrix(pvwMatrix);
		Renderer::Get()->Update(effect->GetPVWMatrixConstant());

		effect->SetEmissiveColor(GetMaterial(idx)->mEmissive);
		Renderer::Get()->Update(effect->GetEmissiveColor());

		// Background color
		SColorF bgColor(mEnvironment->GetSky()->GetBGColor());
		effect->SetSkyBgColor(bgColor.ToArray());
		Renderer::Get()->Update(effect->GetSkyBgColor());

		// Fog distance
		float fogDistance = 10000 * BS;
		effect->SetFogDistance(fogDistance);
		Renderer::Get()->Update(effect->GetFogDistance());
		/*
		unsigned int dayNightRatio = mEnvironment->GetDayNightRatio();
		SColorF sunlight;
		GetSunlightColor(&sunlight, dayNightRatio);
		float dnc[3] = { sunlight.mRed, sunlight.mGreen, sunlight.mBlue };
		effect->SetDayLightColor(dnc);

		SColorF starColor = mEnvironment->GetSky()->GetCurrentStarColor();
		float clr[4] = {
			starColor.mRed, starColor.mGreen, starColor.mBlue, starColor.mAlpha };
		effect->SetStarColor(clr);

		float animationTimer = (Timer::GetRealTime() % 1000000) / 100000.f;
		effect->SetAnimationTimer(animationTimer);

		Vector3<float> epos = mEnvironment->GetPlayer()->GetEyePosition();
		float eyePositionArray[3] = { epos[0], epos[1], epos[2] };
		effect->SetEyePosition(eyePositionArray);

		if (mEnvironment->GetMinimap())
		{
			Vector3<float> minimapYaw = mEnvironment->GetMinimap()->GetYawVec();
			float minimapYawArray[3] = { minimapYaw[0], minimapYaw[1], minimapYaw[2] };
			effect->SetMinimapYaw(minimapYawArray);
		}

		Vector3<short> offset = mEnvironment->GetCameraOffset() * (short)BS;
		float cameraOffsetArray[3] = { (float)offset[0], (float)offset[1], (float)offset[2] };
		effect->SetCameraOffset(cameraOffsetArray);
		*/
		effect->SetTexture(GetMaterial(idx)->GetTexture(TT_DIFFUSE));
		//effect->SetTexture(GetMaterial(idx)->GetTexture(TT_NORMALS));
		//effect->SetTexture(GetMaterial(idx)->GetTexture(TT_COUNT));
	}
}


//! returns the axis aligned bounding box of this node
BoundingBox<float>& ObjectMeshNode::GetBoundingBox()
{
    return mMesh ? mMesh->GetBoundingBox() : mBoundingBox;
}


//! Creates shadow volume scene node as child of this node
//! and returns a pointer to it.
std::shared_ptr<ShadowVolumeNode> ObjectMeshNode::AddShadowVolumeNode(const ActorId actorId,
	Scene* pScene, const std::shared_ptr<BaseMesh>& shadowMesh, bool zfailmethod, float infinity)
{
	/*
	if (!Renderer::Get()->QueryFeature(VDF_STENCIL_BUFFER))
		return nullptr;
	*/
	mShadow = std::shared_ptr<ShadowVolumeNode>(
		new ShadowVolumeNode(actorId, mPVWUpdater,
			shadowMesh ? shadowMesh : mMesh, zfailmethod, infinity));
	shared_from_this()->AttachChild(mShadow);

	return mShadow;
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
int ObjectMeshNode::DetachChild(std::shared_ptr<Node> const& child)
{
	if (child && mShadow == child)
		mShadow = 0;

	if (Node::DetachChild(child))
		return true;

	return false;
}

//! Returns the visual based on the zero based index i. To get the amount 
//! of visuals used by this scene node, use GetVisualCount(). 
//! This function is needed for inserting the node into the scene hierarchy 
//! at an optimal position for minimizing renderstate changes, but can also 
//! be used to directly modify the visual of a scene node.
std::shared_ptr<Visual> const& ObjectMeshNode::GetVisual(unsigned int i)
{
	if (i >= mVisuals.size())
		return nullptr;

	return mVisuals[i];
}

//! return amount of visuals of this scene node.
size_t ObjectMeshNode::GetVisualCount() const
{
	return mVisuals.size();
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& ObjectMeshNode::GetMaterial(unsigned int i)
{
	if (i >= mMesh->GetMeshBufferCount())
		return nullptr;

	return mMesh->GetMeshBuffer(i)->GetMaterial();
}

//! returns amount of materials used by this scene node.
size_t ObjectMeshNode::GetMaterialCount() const
{
	return mMesh->GetMeshBufferCount();
}

//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
\param texture New texture to be used. */
void ObjectMeshNode::SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture)
{
	if (textureLayer >= MATERIAL_MAX_TEXTURES)
		return;

	for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		GetMaterial(i)->SetTexture(textureLayer, texture);
}

//! Sets the material type of all materials in this scene node to a new material type.
/** \param newType New type of material to be set. */
void ObjectMeshNode::SetMaterialType(MaterialType newType)
{
	for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		GetMaterial(i)->mType = newType;
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void ObjectMeshNode::SetReadOnlyMaterials(bool readonly)
{
	mReadOnlyMaterials = readonly;
}

//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool ObjectMeshNode::IsReadOnlyMaterials() const
{
	return mReadOnlyMaterials;
}