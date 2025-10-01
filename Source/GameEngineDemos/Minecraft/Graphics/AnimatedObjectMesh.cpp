// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "AnimatedObjectMesh.h"

#include "Sky.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

#include "Core/OS/OS.h"

#include "Graphic/Scene/Scene.h"

#include "Application/GameApplication.h"

//! constructor
AnimatedObjectMeshNode::AnimatedObjectMeshNode(const ActorId actorId, unsigned int shaderId,
	VisualEnvironment* env, PVWUpdater* updater, const std::shared_ptr<BaseAnimatedMesh>& mesh) : 
	Node(actorId, NT_ANIMATED_MESH), mEnvironment(env), mShaderId(shaderId), mMesh(0), 
	mStartFrame(0), mEndFrame(0), mFramesPerSecond(0.025f), mCurrentFrameNr(0.f), 
	mLastTime(0), mTransitionTime(0), mTransiting(0.f), mTransitingBlend(0.f), 
	mJointMode(JAUOR_NONE), mJointsUsed(false), mLooping(true), mReadOnlyMaterials(false), 
	mRenderFromIdentity(false), mLoopCallBack(0), mPassCount(0), mShadow(0)
{
	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();

	SetMesh(mesh);
}

//! destructor
AnimatedObjectMeshNode::~AnimatedObjectMeshNode()
{
	for (auto visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
}

//! Sets a new mesh
void AnimatedObjectMeshNode::SetMesh(const std::shared_ptr<BaseAnimatedMesh>& mesh)
{
	if (!mesh)
		return; // won't set null mesh

	mMesh = mesh;
	std::vector<std::shared_ptr<BaseMeshBuffer>> meshBuffers;
	for (unsigned int i = 0; i<mMesh->GetMeshBufferCount(); ++i)
		meshBuffers.push_back(mMesh->GetMeshBuffer(i));

    mBoundingBox = mMesh->GetBoundingBox();
		
	for (auto visual : mVisuals)
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

	// clean up joint nodes
	if (mJointsUsed)
	{
		mJointsUsed = false;
		CheckJoints();
	}

	// get start and begin time
	SetAnimationSpeed(mMesh->GetAnimationSpeed());
	SetFrameLoop(0, (unsigned int)mMesh->GetFrameCount());
}

//! Get a mesh
const std::shared_ptr<BaseAnimatedMesh>& AnimatedObjectMeshNode::GetMesh(void)
{
	return mMesh;
}

//! Sets the current frame. From now on the animation is played from this frame.
void AnimatedObjectMeshNode::SetCurrentFrame(float frame)
{
	// if you pass an out of range value, we just clamp it
	mCurrentFrameNr = std::clamp ( frame, (float)mStartFrame, (float)mEndFrame );

	BeginTransition(); //transit to this frame if enabled
}

//! Returns the currently displayed frame number.
float AnimatedObjectMeshNode::GetFrameNr() const
{
	return mCurrentFrameNr;
}

//! Get CurrentFrameNr and update transiting settings
void AnimatedObjectMeshNode::BuildFrameNr(unsigned int timeMs)
{
	if (mTransiting != 0.f)
	{
		mTransitingBlend += timeMs * mTransiting;
		if (mTransitingBlend > 1.f)
		{
			mTransiting = 0.f;
			mTransitingBlend = 0.f;
		}
	}

	if ((mStartFrame==mEndFrame))
	{
		mCurrentFrameNr = (float)mStartFrame; //Support for non animated meshes
	}
	else if (mLooping)
	{
		// play animation looped
		mCurrentFrameNr += timeMs * mFramesPerSecond;

		// We have no interpolation between EndFrame and StartFrame,
		// the last frame must be identical to first one with our current solution.
		if (mFramesPerSecond > 0.f) //forwards...
		{
			if (mCurrentFrameNr > mEndFrame)
			{
				mCurrentFrameNr = (float)mStartFrame;
			}
		}
		else //backwards...
		{
			if (mCurrentFrameNr < mStartFrame)
			{
				mCurrentFrameNr = (float)mEndFrame;
			}
		}
	}
	else
	{
		// play animation non looped
		mCurrentFrameNr += timeMs * mFramesPerSecond;

		if (mFramesPerSecond > 0.f) //forwards...
		{
			if (mCurrentFrameNr > (float)mEndFrame)
			{
				mCurrentFrameNr = (float)mEndFrame;
				if (mLoopCallBack)
					mLoopCallBack->OnAnimationEnd(this);
			}
		}
		else //backwards...
		{
			if (mCurrentFrameNr < (float)mStartFrame)
			{
				mCurrentFrameNr = (float)mStartFrame;
				if (mLoopCallBack)
					mLoopCallBack->OnAnimationEnd(this);
			}
		}
	}
}

std::shared_ptr<BaseMesh> AnimatedObjectMeshNode::GetMeshForCurrentFrame()
{
	if (mMesh->GetMeshType() == MT_SKINNED)
	{
		// As multiple scene nodes may be sharing the same skinned mesh, we have to
		// re-animate it every frame to ensure that this node gets the mesh that it needs.

		std::shared_ptr<SkinnedMesh> skinnedMesh =
			std::dynamic_pointer_cast<SkinnedMesh>(mMesh);

		if (mJointMode == JUOR_CONTROL)//write to mesh
			skinnedMesh->TransferJointsToMesh(mJointChildSceneNodes);
		else
			skinnedMesh->AnimateMesh(GetFrameNr(), 1.0f);

		// Update the skinned mesh for the current joint transforms.
		skinnedMesh->SkinMesh();

		if (mJointMode == JUOR_READ)//read from mesh
		{
			skinnedMesh->RecoverJointsFromMesh(mJointChildSceneNodes);

			//---slow---
			for (unsigned int n = 0; n<mJointChildSceneNodes.size(); ++n)
				if (mJointChildSceneNodes[n]->GetParent() == this)
					mJointChildSceneNodes[n]->UpdateAbsoluteTransformationChildren();
		}

		if (mJointMode == JUOR_CONTROL)
		{
			// For meshes other than JUOR_CONTROL, this is done by calling animateMesh()
			skinnedMesh->UpdateBoundingBox();
		}

		return skinnedMesh;
	}
	else
	{
		int frameNr, frameBlend;		
		frameNr = (int)GetFrameNr();
		frameBlend = (int)(Function<float>::Fract(GetFrameNr()) * 1000.f);

		return mMesh->GetMesh(frameNr, frameBlend, mStartFrame, mEndFrame);
	}
}

//! OnAnimate() is called just before rendering the whole scene.
bool AnimatedObjectMeshNode::OnAnimate(Scene* pScene, unsigned int time)
{
	return Node::OnAnimate(pScene, time);

	if (mLastTime==0)	// first frame
		mLastTime = time;

	// set CurrentFrameNr
	BuildFrameNr(time - mLastTime);

    // update bbox
    if (mMesh)
    {
        std::shared_ptr<BaseMesh> mesh = GetMeshForCurrentFrame();
        if (mesh)
            mBoundingBox = mesh->GetBoundingBox();
    }
	mLastTime = time;

	return Node::OnAnimate(pScene, time);
}

bool AnimatedObjectMeshNode::PreRender(Scene* pScene)
{
	if (IsVisible())
	{
		mCurrentFrameMesh = GetMeshForCurrentFrame();

		// update bbox
		for (auto visual : mVisuals)
			visual->UpdateModelBound();

		// because this node supports rendering of mixed mode meshes consisting of
		// transparent and solid material at the same time, we need to go through all
		// materials, check of what type they are and register this node for the right
		// render pass according to that.

		mPassCount = 0;
		int transparentCount = 0;
		int solidCount = 0;

		std::vector<std::shared_ptr<Material>> materials;
		for (unsigned int i = 0; i<GetMaterialCount(); ++i)
			materials.push_back(GetMaterial(i));

		// count transparent and solid materials in this scene node
		for (unsigned int i = 0; i < materials.size(); ++i)
		{
			if (materials[i]->IsTransparent())
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
bool AnimatedObjectMeshNode::Render(Scene* pScene)
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

void AnimatedObjectMeshNode::UpdateShaderConstants(unsigned int idx, Scene* pScene)
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
BoundingBox<float>& AnimatedObjectMeshNode::GetBoundingBox()
{
    return mBoundingBox;
}

//! Returns the current start frame number.
int AnimatedObjectMeshNode::GetStartFrame() const
{
	return mStartFrame;
}

//! Returns the current start frame number.
int AnimatedObjectMeshNode::GetEndFrame() const
{
	return mEndFrame;
}

//! sets the frames between the animation is looped.
//! the default is 0 - MaximalFrameCount of the mesh.
bool AnimatedObjectMeshNode::SetFrameLoop(int begin, int end)
{
	const int maxFrameCount = (int)mMesh->GetFrameCount() - 1;
	if (end < begin)
	{
		mStartFrame = std::clamp(end, 0, maxFrameCount);
		mEndFrame = std::clamp(begin, mStartFrame, maxFrameCount);
	}
	else
	{
		mStartFrame = std::clamp(begin, 0, maxFrameCount);
		mEndFrame = std::clamp(end, mStartFrame, maxFrameCount);
	}
	if (mFramesPerSecond < 0)
		SetCurrentFrame((float)mEndFrame);
	else
		SetCurrentFrame((float)mStartFrame);

	return true;
}

//! sets the speed with witch the animation is played
void AnimatedObjectMeshNode::SetAnimationSpeed(float framesPerSecond)
{
	mFramesPerSecond = framesPerSecond * 0.001f;
}

float AnimatedObjectMeshNode::GetAnimationSpeed() const
{
	return mFramesPerSecond * 1000.f;
}

//! Sets looping mode which is on by default. If set to false,
//! animations will not be looped.
void AnimatedObjectMeshNode::SetLoopMode(bool playAnimationLooped)
{
	mLooping = playAnimationLooped;
}

//! returns the current loop mode
bool AnimatedObjectMeshNode::GetLoopMode() const
{
	return mLooping;
}

//! Sets a callback interface which will be called if an animation
//! playback has ended. Set this to 0 to disable the callback again.
void AnimatedObjectMeshNode::SetAnimationEndCallback(AnimationObjectEndCallBack* callback)
{
	if (callback == mLoopCallBack.get())
		return;

	mLoopCallBack.reset(callback);
}

//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
std::shared_ptr<BoneNode> AnimatedObjectMeshNode::GetJointNode(const char* jointName)
{
	if (!mMesh || mMesh->GetMeshType() != MT_SKINNED)
	{
		LogWarning("No mesh, or mesh not of skinned mesh type");
		return NULL;
	}

	CheckJoints();

	std::shared_ptr<SkinnedMesh> skinnedMesh =
		std::dynamic_pointer_cast<SkinnedMesh>(mMesh);

	const int number = skinnedMesh->GetJointNumber(jointName);

	if (number == -1)
	{
		LogWarning("Joint with specified name not found in skinned mesh " + std::string(jointName));
		return 0;
	}

	if ((int)mJointChildSceneNodes.size() <= number)
	{
		LogWarning("Joint was found in mesh, but is not loaded into node " + std::string(jointName));
		return 0;
	}

	return mJointChildSceneNodes[number];
}

//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
std::shared_ptr<BoneNode> AnimatedObjectMeshNode::GetJointNode(unsigned int jointID)
{
	if (!mMesh || mMesh->GetMeshType() != MT_SKINNED)
	{
		LogWarning("No mesh, or mesh not of skinned mesh type");
		return NULL;
	}

	CheckJoints();

	if (mJointChildSceneNodes.size() <= jointID)
	{
		LogWarning("Joint not loaded into node");
		return 0;
	}

	return mJointChildSceneNodes[jointID];
}

//! Gets joint count.
unsigned int AnimatedObjectMeshNode::GetJointCount() const
{

	if (!mMesh || mMesh->GetMeshType() != MT_SKINNED)
		return 0;

	std::shared_ptr<SkinnedMesh> skinnedMesh = 
		std::dynamic_pointer_cast<SkinnedMesh>(mMesh);

	return (unsigned int)skinnedMesh->GetJointCount();
}

//! updates the joint positions of this mesh
void AnimatedObjectMeshNode::AnimateJoints(bool calculateAbsolutePositions)
{
	if (mMesh && mMesh->GetMeshType() == MT_SKINNED)
	{
		CheckJoints();
		const float frame = GetFrameNr(); //old?

		std::shared_ptr<SkinnedMesh> skinnedMesh =
			std::dynamic_pointer_cast<SkinnedMesh>(mMesh);

		skinnedMesh->TransferOnlyJointsHintsToMesh(mJointChildSceneNodes);
		skinnedMesh->AnimateMesh(frame, 1.0f);
		skinnedMesh->RecoverJointsFromMesh(mJointChildSceneNodes);

		//-----------------------------------------
		//		Transition
		//-----------------------------------------

		if (mTransiting != 0.f)
		{
			// Init additional matrices
			if (mPretransitingSave.size()<mJointChildSceneNodes.size())
			{
				for (size_t n = mPretransitingSave.size(); n<mJointChildSceneNodes.size(); ++n)
					mPretransitingSave.push_back(Transform());
			}

			for (size_t n = 0; n<mJointChildSceneNodes.size(); ++n)
			{
				//------Position------

				mJointChildSceneNodes[n]->GetAbsoluteTransform().
					SetTranslation(Function<float>::Lerp( 
						mPretransitingSave[n].GetTranslation(),
						mJointChildSceneNodes[n]->GetAbsoluteTransform().GetTranslation(), 
						mTransitingBlend));

				//------Rotation------

				//Code is slow, needs to be fixed up
				const Quaternion<float> rotationStart(
					Rotation<4, float>(mPretransitingSave[n].GetRotation()));
				const Quaternion<float> rotationEnd(
					Rotation<4, float>(mJointChildSceneNodes[n]->GetAbsoluteTransform().GetMatrix()));

				Quaternion<float> qRotation = Slerp(mTransitingBlend, rotationStart, rotationEnd);
				mJointChildSceneNodes[n]->GetAbsoluteTransform().SetRotation(qRotation);

				//------Scale------
				/*
				mJointChildSceneNodes[n]->GetAbsoluteTransform().
					SetScale(Function<float>::Lerp(
						mPretransitingSave[n].GetScale(),
						mJointChildSceneNodes[n]->GetAbsoluteTransform().GetScale(),
						mTransitingBlend));
				*/
			}
		}

		if (calculateAbsolutePositions)
		{
			//---slow---
			for (unsigned int n = 0; n<mJointChildSceneNodes.size(); ++n)
			{
				if (mJointChildSceneNodes[n]->GetParent() == this)
				{
					mJointChildSceneNodes[n]->UpdateAbsoluteTransformationChildren(); //temp, should be an option
				}
			}
		}
	}
}

/*!
*/
void AnimatedObjectMeshNode::CheckJoints()
{
	if (!mMesh || mMesh->GetMeshType() != MT_SKINNED)
		return;

	if (!mJointsUsed)
	{
		for (unsigned int i = 0; i<mJointChildSceneNodes.size(); ++i)
			DetachChild(mJointChildSceneNodes[i]);
		mJointChildSceneNodes.clear();

		//Create joints for SkinnedMesh
		std::shared_ptr<SkinnedMesh> skinnedMesh =
			std::dynamic_pointer_cast<SkinnedMesh>(mMesh);

		skinnedMesh->AddJoints(mJointChildSceneNodes, this, mPVWUpdater);
		skinnedMesh->RecoverJointsFromMesh(mJointChildSceneNodes);

		mJointsUsed = true;
		mJointMode = JAUOR_READ;
	}
}

/*!
*/
void AnimatedObjectMeshNode::BeginTransition()
{
	if (!mJointsUsed)
		return;

	if (mTransitionTime != 0)
	{
		//Check the array is big enough
		if (mPretransitingSave.size()<mJointChildSceneNodes.size())
		{
			for (size_t n = mPretransitingSave.size(); n<mJointChildSceneNodes.size(); ++n)
				mPretransitingSave.push_back(Transform());
		}

		//Copy the position of joints
		for (size_t n = 0; n<mJointChildSceneNodes.size(); ++n)
			mPretransitingSave[n] = mJointChildSceneNodes[n]->GetRelativeTransform();

		mTransiting = mTransitionTime != 0 ? 1.f / (float)mTransitionTime : 0;
	}
	mTransitingBlend = 0.f;
}

//! Set the joint update mode (0-unused, 1-get joints only, 2-set joints only, 3-move and set)
void AnimatedObjectMeshNode::SetJointMode(JointAnimationUpdateOnRender mode)
{
	CheckJoints();
	mJointMode = mode;
}

//! Sets the transition time in seconds (note: This needs to enable joints, and setJointmode maybe set to 2)
//! you must call animateJoints(), or the mesh will not animate
void AnimatedObjectMeshNode::SetTransitionTime(float time)
{
	const unsigned int ttime = (unsigned int)Function<float>::Floor(time*1000.0f);
	if (mTransitionTime == ttime)
		return;
	mTransitionTime = ttime;
	if (ttime != 0)
		SetJointMode(JAUOR_CONTROL);
	else
		SetJointMode(JAUOR_NONE);
}

//! render mesh ignoring its transformation. Used with ragdolls. (culling is unaffected)
void AnimatedObjectMeshNode::SetRenderFromIdentity(bool enable)
{
	mRenderFromIdentity = enable;
}

//! Creates shadow volume scene node as child of this node
//! and returns a pointer to it.
std::shared_ptr<ShadowVolumeNode> AnimatedObjectMeshNode::AddShadowVolumeNode(const ActorId actorId,
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
int AnimatedObjectMeshNode::DetachChild(std::shared_ptr<Node> const& child)
{
	if (child && mShadow == child)
		mShadow = 0;

	if (Node::DetachChild(child))
	{
		if (mJointsUsed) //stop weird bugs caused while changing parents as the joints are being created
		{
			for (unsigned int i = 0; i<mJointChildSceneNodes.size(); ++i)
			{
				if (mJointChildSceneNodes[i] == child)
				{
					mJointChildSceneNodes[i] = 0; //remove link to child
					break;
				}
			}
		}
		return true;
	}

	return false;
}

//! Returns the visual based on the zero based index i. To get the amount 
//! of visuals used by this scene node, use GetVisualCount(). 
//! This function is needed for inserting the node into the scene hierarchy 
//! at an optimal position for minimizing renderstate changes, but can also 
//! be used to directly modify the visual of a scene node.
std::shared_ptr<Visual> const& AnimatedObjectMeshNode::GetVisual(unsigned int i)
{
	if (i >= mVisuals.size())
		return nullptr;

	return mVisuals[i];
}

//! return amount of visuals of this scene node.
size_t AnimatedObjectMeshNode::GetVisualCount() const
{
	return mVisuals.size();
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& AnimatedObjectMeshNode::GetMaterial(unsigned int i)
{
	if (i >= mMesh->GetMeshBufferCount())
		return nullptr;

	return mMesh->GetMeshBuffer(i)->GetMaterial();
}

//! returns amount of materials used by this scene node.
size_t AnimatedObjectMeshNode::GetMaterialCount() const
{
	return mMesh->GetMeshBufferCount();
}

//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
\param texture New texture to be used. */
void AnimatedObjectMeshNode::SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture)
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
void AnimatedObjectMeshNode::SetMaterialType(MaterialType newType)
{
	for (unsigned int i = 0; i<GetMaterialCount(); ++i)
		GetMaterial(i)->mType = newType;
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void AnimatedObjectMeshNode::SetReadOnlyMaterials(bool readonly)
{
	mReadOnlyMaterials = readonly;
}

//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool AnimatedObjectMeshNode::IsReadOnlyMaterials() const
{
	return mReadOnlyMaterials;
}
