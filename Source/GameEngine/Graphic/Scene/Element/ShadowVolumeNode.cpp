// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "ShadowVolumeNode.h"
#include "CameraNode.h"

#include "Graphic/Renderer/Renderer.h"

#include "Core/OS/OS.h"

#include "Graphic/Scene/Scene.h"


//! constructor
ShadowVolumeNode::ShadowVolumeNode(const ActorId actorId, PVWUpdater* updater, 
	const std::shared_ptr<BaseMesh>& shadowMesh, bool zfailmethod, float infinity)
:	Node(actorId, NT_SHADOW_VOLUME), mShadowMesh(0), mIndexCount(0), 
	mVertexCount(0), mShadowVolumesUsed(0), mInfinity(infinity), mUseZFailMethod(zfailmethod)
{
	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();

	SetShadowMesh(shadowMesh);
}

ShadowVolumeNode::~ShadowVolumeNode()
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
}

void ShadowVolumeNode::SetShadowMesh(const std::shared_ptr<BaseMesh>& mesh)
{
	if (mShadowMesh == mesh)
		return;

	mShadowMesh = mesh;

	if (mShadowMesh)
	{
		MeshFactory mf;

		for (auto const& visual : mVisuals)
			mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
		mVisuals.clear();
		for (unsigned int i = 0; i<mShadowMesh->GetMeshBufferCount(); ++i)
		{
			const std::shared_ptr<BaseMeshBuffer>& meshBuffer = mShadowMesh->GetMeshBuffer(i);
			if (meshBuffer)
			{
				mBlendStates.push_back(std::make_shared<BlendState>());
				mDepthStencilStates.push_back(std::make_shared<DepthStencilState>());

				// Create the visual effect.  The world up-direction is (0,1,0).  Choose
				// the light to point down..
				meshBuffer->GetMaterial()->mEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
				meshBuffer->GetMaterial()->mAmbient = { 0.5f, 0.5f, 0.5f, 1.0f };
				meshBuffer->GetMaterial()->mDiffuse = { 0.5f, 0.5f, 0.5f, 1.0f };
				meshBuffer->GetMaterial()->mSpecular = { 1.0f, 1.0f, 1.0f, 0.75f };

				std::shared_ptr<Lighting> lighting = std::make_shared<Lighting>();
				std::shared_ptr<LightCameraGeometry> geometry = std::make_shared<LightCameraGeometry>();

				std::vector<std::string> path;
#if defined(_OPENGL_)
				path.push_back("Effects/PointLightTextureEffectVS.glsl");
				path.push_back("Effects/PointLightTextureEffectPS.glsl");
#else
				path.push_back("Effects/PointLightTextureEffectVS.hlsl");
				path.push_back("Effects/PointLightTextureEffectPS.hlsl");
#endif
				std::shared_ptr<ResHandle> resHandle =
					ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

				const std::shared_ptr<ShaderResourceExtraData>& extra =
					std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
				if (!extra->GetProgram())
					extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

				mEffect = std::make_shared<PointLightTextureEffect>(
					ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()),
					mPVWUpdater->GetUpdater(), meshBuffer->GetMaterial(), lighting,
					geometry, std::make_shared<Texture2>(DF_UNKNOWN, 0, 0, true), 
					SamplerState::MIN_L_MAG_L_MIP_L, SamplerState::WRAP, SamplerState::WRAP);

				std::shared_ptr<Visual> visual = std::make_shared<Visual>(
					meshBuffer->GetVertice(), meshBuffer->GetIndice(), mEffect);
				visual->UpdateModelBound();
				mVisuals.push_back(visual);
				mPVWUpdater->Subscribe(mWorldTransform, mEffect->GetPVWMatrixConstant());
			}
		}

        mBoundingBox = mShadowMesh->GetBoundingBox();
	}
}

//! returns the axis aligned bounding box of this node
BoundingBox<float>& ShadowVolumeNode::GetBoundingBox()
{
    return mBoundingBox;
}

void ShadowVolumeNode::CreateShadowVolume(const Vector3<float>& light, bool isDirectional)
{
	ShadowVolume* svp = 0;
	BoundingSphere* bs = 0;

	// builds the shadow volume and adds it to the shadow volume list.
	if (mShadowVolumes.size() > mShadowVolumesUsed)
	{
		// get the next unused buffer

		svp = &mShadowVolumes[mShadowVolumesUsed];
		bs = &mShadowBS[mShadowVolumesUsed];
	}
	else
	{
		mShadowVolumes.push_back(ShadowVolume());
		svp = &mShadowVolumes.back();

        mShadowBS.push_back(BoundingSphere());
		bs = &mShadowBS.back();
	}
	++mShadowVolumesUsed;

	// We use triangle lists
	unsigned int numEdges = 0;

	numEdges=CreateEdgesAndCaps(light, svp, bs);

	// for all edges add the near->far quads
	for (unsigned int i=0; i<numEdges; ++i)
	{
		Vector3<float> &v1 = mVertices[mEdges[2*i+0]];
		Vector3<float> &v2 = mVertices[mEdges[2*i+1]];
		Vector3<float> v3((v1+(v1 - light))*mInfinity);
		Vector3<float> v4((v2+(v2 - light))*mInfinity);
		Normalize(v3);
		Normalize(v4);

		// Add a quad (two triangles) to the vertex list
		svp->push_back(v1);
		svp->push_back(v2);
		svp->push_back(v3);

		svp->push_back(v2);
		svp->push_back(v4);
		svp->push_back(v3);
	}
}


#define _USE_ADJACENCY
#define _USE_REVERSE_EXTRUDED

unsigned int ShadowVolumeNode::CreateEdgesAndCaps(const Vector3<float>& light, ShadowVolume* svp, BoundingSphere* bs)
{
	unsigned int numEdges=0;
	const unsigned int faceCount = mIndexCount / 3;

	bs->SetCenter(Vector4<float>::Zero());

	// Check every face if it is front or back facing the light.
	for (unsigned int i=0; i<faceCount; ++i)
	{
		const Vector3<float> v0 = mVertices[mIndices[3*i+0]];
		const Vector3<float> v1 = mVertices[mIndices[3*i+1]];
		const Vector3<float> v2 = mVertices[mIndices[3*i+2]];

#ifdef _USE_REVERSE_EXTRUDED
		//! Test if the triangle would be front or backfacing from any point.
		Vector3<float> normal = Cross(v1 - v0, v2 - v2);
		Normalize(normal);
		mFaceData[i] = Dot(normal, light) <= 0.0f;
#else
		//! Test if the triangle would be front or backfacing from any point.
		Vector3<float> normal = Cross(v1 - v2, v2 - v0);
		Normalize(normal);
		mFaceData[i] = Dot(normal, light) <= 0.0f;
#endif

		if (mUseZFailMethod && mFaceData[i])
		{
			// add front cap from light-facing faces
			svp->push_back(v2);
			svp->push_back(v1);
			svp->push_back(v0);

			// add back cap
			Vector3<float> i0((v0+(v0-light))*mInfinity);
			Vector3<float> i1((v1+(v1-light))*mInfinity);
			Vector3<float> i2((v2+(v2-light))*mInfinity);
			Normalize(i0);
			Normalize(i1);
			Normalize(i2);

			svp->push_back(i0);
			svp->push_back(i1);
			svp->push_back(i2);

			bs->ComputeFromData(1, sizeof(Vector3<float>), reinterpret_cast<const char*>(&i0));
			bs->ComputeFromData(1, sizeof(Vector3<float>), reinterpret_cast<const char*>(&i1));
			bs->ComputeFromData(1, sizeof(Vector3<float>), reinterpret_cast<const char*>(&i2));
		}
	}

	// Create edges
	for (unsigned int i=0; i<faceCount; ++i)
	{
		// check all front facing faces
		if (mFaceData[i] == true)
		{
			const unsigned int wFace0 = mIndices[3*i+0];
			const unsigned int wFace1 = mIndices[3*i+1];
			const unsigned int wFace2 = mIndices[3*i+2];

			const unsigned int adj0 = mAdjacency[3*i+0];
			const unsigned int adj1 = mAdjacency[3*i+1];
			const unsigned int adj2 = mAdjacency[3*i+2];

			// add edges if face is adjacent to back-facing face
			// or if no adjacent face was found
#ifdef _USE_ADJACENCY
			if (adj0 == i || mFaceData[adj0] == false)
#endif
			{
				// add edge v0-v1
				mEdges[2*numEdges+0] = wFace0;
				mEdges[2*numEdges+1] = wFace1;
				++numEdges;
			}

#ifdef _USE_ADJACENCY
			if (adj1 == i || mFaceData[adj1] == false)
#endif
			{
				// add edge v1-v2
				mEdges[2*numEdges+0] = wFace1;
				mEdges[2*numEdges+1] = wFace2;
				++numEdges;
			}

#ifdef _USE_ADJACENCY
			if (adj2 == i || mFaceData[adj2] == false)
#endif
			{
				// add edge v2-v0
				mEdges[2*numEdges+0] = wFace2;
				mEdges[2*numEdges+1] = wFace0;
				++numEdges;
			}
		}
	}
	return numEdges;
}

void ShadowVolumeNode::UpdateShadowVolumes(Scene *pScene)
{
	const unsigned int oldIndexCount = mIndexCount;
	const unsigned int oldVertexCount = mVertexCount;

	const BaseMesh* const mesh = mShadowMesh.get();
	if (!mesh) return;

	// create as much shadow volumes as there are lights but
	// do not ignore the max light settings.
	/*
	const unsigned int lightCount = Renderer::Get()->GetDynamicLightCount();
	if (!lightCount)
		return;
	*/
	// calculate total amount of vertices and indices

	mVertexCount = 0;
	mIndexCount = 0;
	mShadowVolumesUsed = 0;

	unsigned int i;
	unsigned int totalVertices = 0;
	unsigned int totalIndices = 0;
	const unsigned int bufcnt = (unsigned int)mesh->GetMeshBufferCount();

	for (i=0; i<bufcnt; ++i)
	{
		const BaseMeshBuffer* buf = mesh->GetMeshBuffer(i).get();
		totalIndices += buf->GetNumElements();
		totalVertices += buf->GetNumElements();
	}

	// copy mesh
	for (unsigned int i = 0; i<(unsigned int)mesh->GetMeshBufferCount(); ++i)
	{
		unsigned int const* index = mVisuals[i]->GetIndexBuffer()->Get<unsigned int>();
		unsigned int numElements = mVisuals[i]->GetIndexBuffer()->GetNumElements();
		for (unsigned int i = 0; i < numElements; ++i, ++index)
			mIndices[mIndexCount++] = *index + mVertexCount;

		struct Vertex
		{
			Vector3<float> position;
		};
		Vertex* vertex = mVisuals[i]->GetVertexBuffer()->Get<Vertex>();
		numElements = mVisuals[i]->GetVertexBuffer()->GetNumElements();
		for (unsigned int i = 0; i < numElements; ++i, ++index)
			mVertices[mVertexCount++] = vertex[i].position;
	}

	// recalculate adjacency if necessary
	if (oldVertexCount != mVertexCount || oldIndexCount != mIndexCount)
		CalculateAdjacency();

	//Matrix4x4<float> toWorld, fromWorld;
	//GetParent()->GetAbsoluteTransform(&toWorld, &fromWorld);
	//toWorld.MakeInverse();

	const Vector3<float> parentpos = GetParent()->GetAbsoluteTransform().GetTranslation();

	// TODO: Only correct for point lights.
	/*
	for (i=0; i<lightCount; ++i)
	{
		const Light& dl = Renderer::Get()->GetDynamicLight(i);
		Vector3<float> lpos = dl.mLighting->mPosition;
		if (dl.mLighting->mCastShadows &&
			fabs(Length(lpos - parentpos)) <= (dl.mLighting->mRadius*dl.mLighting->mRadius*4.0f))
		{
			toWorld.TransformVect(lpos);
			CreateShadowVolume(lpos);
		}
	}
	*/
}

//! pre render method
bool ShadowVolumeNode::PreRender(Scene* pScene)
{
	if (IsVisible())
	{
		// register according to material types counted
		if (!pScene->IsCulled(this))
			pScene->AddToRenderQueue(RP_SHADOW, shared_from_this());
	}

	return Node::PreRender(pScene);
}

//! renders the node.
bool ShadowVolumeNode::Render(Scene* pScene)
{
	if (!mShadowVolumesUsed || !Renderer::Get())
		return false;

	//Renderer::Get()->SetTransform(TS_WORLD, toWorld);

	for (unsigned int i=0; i<mShadowVolumesUsed; ++i)
	{
		bool drawShadow = true;

		if (mUseZFailMethod && pScene->GetActiveCamera())
		{
			// Disable shadows drawing, when back cap is behind of ZFar plane.

			const float* frustum = pScene->GetActiveCamera()->Get()->GetFrustum();

			//Matrix4x4<float> invTrans(toWorld, Matrix4x4<float>::EM4CONST_INVERSE);
			//frust.Transform(invTrans);

			Vector3<float> edges[8];
			//mShadowBBox[i].GetEdges(edges);

			Vector3<float> largestEdge = edges[0];
			float maxDistance = Length(pScene->GetActiveCamera()->GetAbsoluteTransform().GetTranslation() - edges[0]);
			float curDistance = 0.f;

			for(int j = 1; j < 8; ++j)
			{
				curDistance = Length(pScene->GetActiveCamera()->GetAbsoluteTransform().GetTranslation()  - edges[j]);
				if(curDistance > maxDistance)
				{
					maxDistance = curDistance;
					largestEdge = edges[j];
				}
			}
			/*
			if (!(frustum[Camera::VF_DMAX].ClassifyPointRelation(largestEdge) != ISREL3D_FRONT))
				drawShadow = false;
			*/
		}
		/*
		if(!drawShadow)
		{
			std::vector<Vector3<float>> triangles;
			Renderer::Get()->DrawStencilShadowVolume(triangles, mUseZFailMethod, DebugDataVisible());
		}
		else Renderer::Get()->DrawStencilShadowVolume(mShadowVolumes[i], mUseZFailMethod, DebugDataVisible());
		*/
	}
	return true;
}

//! Generates adjacency information based on mesh indices.
void ShadowVolumeNode::CalculateAdjacency()
{
	// go through all faces and fetch their three neighbours
	for (unsigned int f=0; f<mIndexCount; f+=3)
	{
		for (unsigned int edge = 0; edge<3; ++edge)
		{
			const Vector3<float>& v1 = mVertices[mIndices[f+edge]];
			const Vector3<float>& v2 = mVertices[mIndices[f+((edge+1)%3)]];

			// now we search an_O_ther _F_ace with these two
			// vertices, which is not the current face.
			unsigned int of;

			for (of=0; of<mIndexCount; of+=3)
			{
				// only other faces
				if (of != f)
				{
					bool cnt1 = false;
					bool cnt2 = false;

					for (int e=0; e<3; ++e)
					{
						if (v1 == mVertices[mIndices[of+e]])
							cnt1=true;

						if (v2 == mVertices[mIndices[of+e]])
							cnt2=true;
					}
					// one match for each vertex, i.e. edge is the same
					if (cnt1 && cnt2)
						break;
				}
			}

			// no adjacent edges -> store face number, else store adjacent face
			if (of >= mIndexCount)
				mAdjacency[f + edge] = f/3;
			else
				mAdjacency[f + edge] = of/3;
		}
	}
}