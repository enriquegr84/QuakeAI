/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "../../MinecraftEvents.h"

#include "ContentVisualActiveObject.h"
#include "ContentVisualSimpleObject.h"

#include "VisualPlayer.h"

#include "../../Games/Map/Map.h"

#include "../../Games/Actors/Player.h"
#include "../../Games/Environment/VisualEnvironment.h"

#include "../../Physics/Collision.h"

#include "../../Graphics/Map/Minimap.h"
#include "../../Graphics/PlayerCamera.h"
#include "../../Graphics/MeshUtil.h"
#include "../../Graphics/Shader.h"
#include "../../Graphics/Node.h"
#include "../../Graphics/ObjectMesh.h"
#include "../../Graphics/AnimatedObjectMesh.h"

#include "../../Utils/Util.h"

#include "Core/IO/ResourceCache.h"
#include "Core/Utility/Serialize.h"

#include "Graphic/Scene/Scene.h"
#include "Graphic/Scene/Mesh/NormalMesh.h"
#include "Graphic/Scene/Mesh/AnimatedMesh.h"
#include "Graphic/Scene/Mesh/MeshFileLoader.h"
#include "Graphic/Scene/Element/RootNode.h"
#include "Graphic/Scene/Element/CameraNode.h"
#include "Graphic/Scene/Element/BillboardNode.h"
#include "Graphic/Scene/Element/DummyTransformationNode.h"

#include "Application/GameApplication.h"

struct ToolCapabilities;

std::unordered_map<unsigned short, VisualActiveObject::Factory> VisualActiveObject::mTypes;

template<typename T>
void SmoothTranslator<T>::Init(T current)
{
	valOld = current;
	valCurrent = current;
	valTarget = current;
	animTime = 0;
	animTimeCounter = 0;
	aimIsEnd = true;
}

template<typename T>
void SmoothTranslator<T>::Update(T newTarget, bool isEndPosition, float updateInterval)
{
	aimIsEnd = isEndPosition;
	valOld = valCurrent;
	valTarget = newTarget;
	if (updateInterval > 0) 
    {
		animTime = updateInterval;
	} 
    else 
    {
		if (animTime < 0.001f || animTime > 1.f)
			animTime = animTimeCounter;
		else
			animTime = animTime * 0.9f + animTimeCounter * 0.1f;
	}
	animTimeCounter = 0;
}

template<typename T>
void SmoothTranslator<T>::Translate(float dTime)
{
	animTimeCounter = animTimeCounter + dTime;
	T valDiff = valTarget - valOld;
	float moveRatio = 1.f;
	if (animTime > 0.001f)
		moveRatio = animTimeCounter / animTime;
	float moveEnd = aimIsEnd ? 1.f : 1.5f;

	// Move a bit less than should, to avoid oscillation
	moveRatio = std::min(moveRatio * 0.8f, moveEnd);
	valCurrent = valOld + valDiff * moveRatio;
}

void SmoothTranslatorWrapped::Translate(float dTime)
{
	animTimeCounter = animTimeCounter + dTime;
	float valDiff = std::abs(valTarget - valOld);
	if (valDiff > 180.f)
		valDiff = 360.f - valDiff;

	float moveRatio = 1.0;
	if (animTime > 0.001)
		moveRatio = animTimeCounter / animTime;
	float moveEnd = aimIsEnd ? 1.f : 1.5f;

	// Move a bit less than should, to avoid oscillation
	moveRatio = std::min(moveRatio * 0.8f, moveEnd);
	WrappedApproachShortest(valCurrent, valTarget, valDiff * moveRatio, 360.f);
}

void SmoothTranslatorWrappedVector3Float::Translate(float dTime)
{
	animTimeCounter = animTimeCounter + dTime;

	Vector3<float> valDiffVector3Float;
	valDiffVector3Float[0] = std::abs(valTarget[0] - valOld[0]);
	valDiffVector3Float[1] = std::abs(valTarget[1] - valOld[1]);
	valDiffVector3Float[2] = std::abs(valTarget[2] - valOld[2]);

	if (valDiffVector3Float[0] > 180.f)
		valDiffVector3Float[0] = 360.f - valDiffVector3Float[0];

	if (valDiffVector3Float[1] > 180.f)
		valDiffVector3Float[1] = 360.f - valDiffVector3Float[1];

	if (valDiffVector3Float[2] > 180.f)
		valDiffVector3Float[2] = 360.f - valDiffVector3Float[2];

	float moveRatio = 1.0;
	if (animTime > 0.001)
		moveRatio = animTimeCounter / animTime;
	float moveEnd = aimIsEnd ? 1.f : 1.5f;

	// Move a bit less than should, to avoid oscillation
	moveRatio = std::min(moveRatio * 0.8f, moveEnd);
	WrappedApproachShortest(valCurrent[0], valTarget[0],
		valDiffVector3Float[0] * moveRatio, 360.f);

	WrappedApproachShortest(valCurrent[1], valTarget[1],
		valDiffVector3Float[1] * moveRatio, 360.f);

	WrappedApproachShortest(valCurrent[2], valTarget[2],
		valDiffVector3Float[2] * moveRatio, 360.f);
}

/*
	Other stuff
*/

static void SetBillboardTextureMatrix(std::shared_ptr<BillboardNode> bill,
		float txs, float tys, int col, int row)
{
	std::shared_ptr<Material> material = bill->GetMaterial(0);
	Transform& transform = material->GetTextureTransform(0);
	transform.Set2DTranslation(txs*col, tys*row);
    transform.Set2DScale(txs, tys);
}

// Evaluate transform chain recursively; game engine does not do this for us
static void UpdatePositionRecursive(Spatial* node)
{
	Spatial* parent = node->GetParent();
	if (parent)
		UpdatePositionRecursive(parent);
	node->UpdateAbsoluteTransform();
}

/*
	GenericVisualActiveObject
*/

GenericVisualActiveObject::GenericVisualActiveObject(VisualEnvironment* env)
    : VisualActiveObject(0, env)
{
    VisualActiveObject::RegisterType(GetType(), Create);
}

bool GenericVisualActiveObject::GetCollisionBox(BoundingBox<float>* toset) const
{
	if (mProp.physical)
	{
		//update collision box
		toset->mMinEdge = mProp.collisionBox.mMinEdge * BS;
		toset->mMaxEdge = mProp.collisionBox.mMaxEdge * BS;

		toset->mMinEdge += mPosition;
		toset->mMaxEdge += mPosition;

		return true;
	}

	return false;
}

bool GenericVisualActiveObject::CollideWithObjects() const
{
	return mProp.collideWithObjects;
}

void GenericVisualActiveObject::Initialize(const std::string& data)
{
	LogInformation("GenericVisualActiveObject: Got init data");
	ProcessInitData(data);

	if (mIsPlayer) 
    {
		// Check if it's the current player
		VisualPlayer* player = mEnvironment->GetPlayer();
		if (player && strcmp(player->GetName(), mName.c_str()) == 0) 
        {
			mIsVisualPlayer = true;
			mIsVisible = false;
			player->SetVAO(this);

			mProp.showOnMinimap = false;
		}
	}

	mEnableShaders = Settings::Get()->GetBool("enable_shaders");
}

void GenericVisualActiveObject::ProcessInitData(const std::string& data)
{
	std::istringstream is(data, std::ios::binary);
	const uint8_t version = ReadUInt8(is);

	if (version < 1) 
    {
		LogError("GenericVisualActiveObject: Unsupported init data version");
		return;
	}

	// PROTOCOL_VERSION >= 37
	mName = DeserializeString16(is);
	mIsPlayer = ReadUInt8(is);
	mId = ReadUInt16(is);
	mPosition = ReadV3Float(is);
	mRotation = ReadV3Float(is);
	mHP = ReadUInt16(is);

	const uint8_t numMessages = ReadUInt8(is);

	for (int i = 0; i < numMessages; i++) 
    {
		std::string message = DeserializeString32(is);
		ProcessMessage(message);
	}

	mRotation = WrapDegrees360(mRotation);
	mPosTranslator.Init(mPosition);
	mRotTranslator.Init(mRotation);
	UpdateNodePosition();
}

GenericVisualActiveObject::~GenericVisualActiveObject()
{
	RemoveFromScene(true);
}

bool GenericVisualActiveObject::GetSelectionBox(BoundingBox<float>* toset) const
{
	if (!mProp.isVisible || !mIsVisible || mIsVisualPlayer || !mProp.pointable)
		return false;

	*toset = mSelectionBox;
	return true;
}

const Vector3<float> GenericVisualActiveObject::GetPosition() const
{
	if (!GetParent())
		return mPosTranslator.valCurrent;

	// Calculate real position in world based on MatrixNode
	if (mTransformNode) 
    {
		Vector3<short> cameraOffset = mEnvironment->GetCameraOffset();
		return mTransformNode->GetAbsoluteTransform().GetTranslation() +
            Vector3<float>{(float)cameraOffset[0], (float)cameraOffset[1], (float)cameraOffset[2]} * BS;
	}

	return mPosition;
}

const bool GenericVisualActiveObject::IsImmortal()
{
	return ItemGroupGet(GetGroups(), "Immortal");
}

Transform& GenericVisualActiveObject::GetRelativeTransform()
{
    LogAssert(mTransformNode, "invalid node");
    return mTransformNode->GetRelativeTransform();
}

Transform* GenericVisualActiveObject::GetAbsoluteTransform() const
{
    if (!mTransformNode)
        return nullptr;
    return &mTransformNode->GetAbsoluteTransform();
}

std::shared_ptr<Node> GenericVisualActiveObject::GetSceneNode() const
{
	if (mMeshNode)
		return mMeshNode;

	if (mAnimatedMeshNode)
		return mAnimatedMeshNode;
    
	if (mWieldMeshNode)
		return mWieldMeshNode;

	if (mSpriteNode)
		return mSpriteNode;

	return NULL;
}

std::shared_ptr<AnimatedObjectMeshNode> GenericVisualActiveObject::GetAnimatedMeshSceneNode() const
{
	return mAnimatedMeshNode;
}

void GenericVisualActiveObject::SetChildrenVisible(bool toset)
{
	for (unsigned short vaoId : mAttachmentChildIds)
    {
		GenericVisualActiveObject* obj = mEnvironment->GetGenericVAO(vaoId);
		if (obj) 
        {
			// Check if the entity is forced to appear in first person.
			obj->SetVisible(obj->mForceVisible ? true : toset);
		}
	}
}

void GenericVisualActiveObject::SetAttachment(int parentId, const std::string& bone,
		Vector3<float> position, EulerAngles<float> rotation, bool forceVisible)
{
	int oldParent = mAttachmentParentId;
	mAttachmentParentId = parentId;
	mAttachmentBone = bone;
	mAttachmentPosition = position;
	mAttachmentRotation = rotation;
	mForceVisible = forceVisible;

	VisualActiveObject* parent = mEnvironment->GetActiveObject(parentId);
	if (parentId != oldParent) 
    {
		if (auto* obj = mEnvironment->GetActiveObject(oldParent))
            obj->RemoveAttachmentChild(mId);
		if (parent)
			parent->AddAttachmentChild(mId);
	}
	UpdateAttachments();

	// Forcibly show attachments if required by set_attach
	if (mForceVisible) 
    {
		mIsVisible = true;
	} 
    else if (!mIsVisualPlayer) 
    {
		// Objects attached to the local player should be hidden in first person
		mIsVisible = !mAttachedToLocal || 
           mEnvironment->GetPlayerCamera()->GetCameraMode() != CAMERA_MODE_FIRST;
		mForceVisible = false;
	} 
    else 
    {
		// Local players need to have this set,
		// otherwise first person attachments fail.
		mIsVisible = true;
	}
}

void GenericVisualActiveObject::GetAttachment(int* parentId, std::string* bone, 
    Vector3<float>* position, EulerAngles<float>* rotation, bool* forceVisible) const
{
	*parentId = mAttachmentParentId;
	*bone = mAttachmentBone;
	*position = mAttachmentPosition;
	*rotation = mAttachmentRotation;
	*forceVisible = mForceVisible;
}

void GenericVisualActiveObject::ClearChildAttachments()
{
	// Cannot use for-loop here: SetAttachment() modifies 'mAttachmentChildIds'!
	while (!mAttachmentChildIds.empty()) 
    {
		int childId = *mAttachmentChildIds.begin();

		if (VisualActiveObject* child = mEnvironment->GetActiveObject(childId))
			child->SetAttachment(0, "", Vector3<float>(), Vector3<float>(), false);

		RemoveAttachmentChild(childId);
	}
}

void GenericVisualActiveObject::ClearParentAttachment()
{
	if (mAttachmentParentId)
		SetAttachment(0, "", mAttachmentPosition, mAttachmentRotation, false);
	else
		SetAttachment(0, "", Vector3<float>::Zero(), EulerAngles<float>(), false);
}

void GenericVisualActiveObject::AddAttachmentChild(int childId)
{
	mAttachmentChildIds.insert(childId);
}

void GenericVisualActiveObject::RemoveAttachmentChild(int childId)
{
	mAttachmentChildIds.erase(childId);
}

VisualActiveObject* GenericVisualActiveObject::GetParent() const
{
	return mAttachmentParentId ? mEnvironment->GetActiveObject(mAttachmentParentId) : nullptr;
}

void GenericVisualActiveObject::RemoveFromScene(bool permanent)
{
	// Should be true when removing the object permanently
	// and false when refreshing (eg: updating visuals)
	if (mEnvironment && permanent) 
    {
		// The visual does not know whether this object does re-appear to
		// a later time, thus do not clear child attachments.
		ClearParentAttachment();
	}

	std::shared_ptr<Scene> pScene = GameApplication::Get()->GetHumanView()->mScene;
	if (mMeshNode) 
    {
		pScene->RemoveSceneNode(mMeshNode->GetId());
		mMeshNode = nullptr;
	} 
    else if (mAnimatedMeshNode)	
    {
		pScene->RemoveSceneNode(mAnimatedMeshNode->GetId());
		mAnimatedMeshNode = nullptr;
	} 
    else if (mWieldMeshNode) 
    {
		pScene->RemoveSceneNode(mWieldMeshNode->GetId());
		mWieldMeshNode = nullptr;
	} 
    else if (mSpriteNode) 
    {
		pScene->RemoveSceneNode(mSpriteNode->GetId());
		mSpriteNode = nullptr;
	}

	if (mTransformNode) 
    {
		mTransformNode->DetachParent();
        mTransformNode = nullptr;
	}

	if (mNameTag) 
    {
        mEnvironment->GetPlayerCamera()->RemoveNameTag(mNameTag);
		mNameTag = nullptr;
	}

	if (mMarker && mEnvironment->GetMinimap())
        mEnvironment->GetMinimap()->RemoveMarker(&mMarker);
}

void GenericVisualActiveObject::AddToScene()
{
	if (GetSceneNode() != NULL)
		return;

	mVisualsExpired = false;

	if (!mProp.isVisible)
		return;

	LogInformation("GenericVisualActiveObject::AddToScene(): " + mProp.visual);

	BaseShaderSource* shaderSrc = mEnvironment->GetShaderSource();
	TileMaterialType materialType;

	if (mProp.shaded && mProp.glow == 0)
		materialType = (mProp.useTextureAlpha) ? TILE_MATERIAL_ALPHA : TILE_MATERIAL_BASIC;
	else
		materialType = (mProp.useTextureAlpha) ? TILE_MATERIAL_PLAIN_ALPHA : TILE_MATERIAL_PLAIN;

	mShaderId = shaderSrc->GetShader("Object", materialType, NDT_NORMAL);
	mMaterialType = shaderSrc->GetShaderInfo(mShaderId).material;

	auto GrabTransformNode = [this] 
    {
		mTransformNode = GameApplication::Get()->GetHumanView()->mScene->AddDummyTransformationNode(0);
	};

	auto SetSceneNodeMaterial = [this] (std::shared_ptr<Node> node)
    {
        node->GetMaterial(0)->mLighting = false;
        node->GetMaterial(0)->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
		//node->SetMaterialFlag(MF_FOG_ENABLE, true);
		node->SetMaterialType(mMaterialType);
		node->GetMaterial(0)->mTypeParam2 = mShaderId;

		if (mEnableShaders) 
        {
            node->GetMaterial(0)->mShadingModel = SM_BLINN;
			//node->SetMaterialFlag(MF_NORMALIZE_NORMALS, true);
		}
	};

	std::shared_ptr<Scene> pScene = GameApplication::Get()->GetHumanView()->mScene;
	if (mProp.visual == "sprite") 
    {
        BaseTextureSource* texSrc = mEnvironment->GetTextureSource();

        GrabTransformNode();
		mSpriteNode = pScene->AddBillboardNode(
            mTransformNode, texSrc->GetTextureForMesh("unknown_node.png"), Vector2<float>{1, 1}, -1);

		SetSceneNodeMaterial(mSpriteNode);

        mSpriteNode->SetSize(Vector2<float>{mProp.visualSize[0], mProp.visualSize[1]} *BS);
		{
			const float txs = 1.0 / 1;
			const float tys = 1.0 / 1;
			SetBillboardTextureMatrix(mSpriteNode, txs, tys, 0, 0);
		}
	} 
    else if (mProp.visual == "upright_sprite") 
    {
		GrabTransformNode();
		std::shared_ptr<BaseMesh> mesh(new NormalMesh());
		float dx = BS * mProp.visualSize[0] / 2;
        float dy = BS * mProp.visualSize[1] / 2;
		SColorF c(0xFFFFFFFF);

        struct Vertex
        {
            Vector3<float> position;
            Vector2<float> tcoord;
            Vector4<float> color;
			Vector3<float> normal;
        };
        VertexFormat vformat;
        vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
        vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
        vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
		vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);

		{ // Front
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));

            // fill vertices
            meshBuffer->Position(0) = Vector3<float>{ -dx, -dy, 0 };
            meshBuffer->Position(1) = Vector3<float>{ dx, -dy, 0 };
            meshBuffer->Position(2) = Vector3<float>{ dx, dy, 0 };
            meshBuffer->Position(3) = Vector3<float>{ -dx, dy, 0 };

            meshBuffer->Normal(0) = Vector3<float>{ 0, 0, 1 };
            meshBuffer->Normal(1) = Vector3<float>{ 0, 0, 1 };
            meshBuffer->Normal(2) = Vector3<float>{ 0, 0, 1 };
            meshBuffer->Normal(3) = Vector3<float>{ 0, 0, 1 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
				meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 1.0f, 1.0f };
				meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 0.0f, 1.0f };
				meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 0.0f, 0.0f };
				meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 1.0f, 0.0f };

                meshBuffer->Color(0, 0 + i) = c.ToArray();
                meshBuffer->Color(0, 1 + i) = c.ToArray();
                meshBuffer->Color(0, 2 + i) = c.ToArray();
                meshBuffer->Color(0, 3 + i) = c.ToArray();
            }

            // fill indices
            int vertices = 0;
            for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i += 2, vertices += 4)
            {
                meshBuffer->GetIndice()->SetTriangle(i,
                    (unsigned int)0 + vertices, (unsigned int)1 + vertices, (unsigned int)2 + vertices);
                meshBuffer->GetIndice()->SetTriangle(i + 1,
                    (unsigned int)2 + vertices, (unsigned int)3 + vertices, (unsigned int)0 + vertices);
            }

			if (mIsPlayer) 
            {
				// Move minimal Y position to 0 (feet position)
				for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
					meshBuffer->Position(i)[1] += dy;
			}

			// Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
            //meshBuffer->GetMaterial()->SetFlag(MF_FOG_ENABLE, true);
            meshBuffer->GetMaterial()->mType = mMaterialType;
			meshBuffer->GetMaterial()->mTypeParam2 = mShaderId;

			if (mEnableShaders) 
            {
                meshBuffer->GetMaterial()->mEmissive = c.ToArray();
                meshBuffer->GetMaterial()->mShadingModel = ShadingModel::SM_BLINN;
                //meshBuffer->GetMaterial()->SetFlag(MF_NORMALIZE_NORMALS, true);
			}

			// Add to mesh
			mesh->AddMeshBuffer(meshBuffer);
		}
		{ // Back

            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));

            // fill vertices
            meshBuffer->Position(0) = Vector3<float>{ dx, -dy, 0 };
            meshBuffer->Position(1) = Vector3<float>{ -dx, -dy, 0 };
            meshBuffer->Position(2) = Vector3<float>{ -dx, dy, 0 };
            meshBuffer->Position(3) = Vector3<float>{ dx, dy, 0 };

            meshBuffer->Normal(0) = Vector3<float>{ 0, 0, -1 };
            meshBuffer->Normal(1) = Vector3<float>{ 0, 0, -1 };
            meshBuffer->Normal(2) = Vector3<float>{ 0, 0, -1 };
            meshBuffer->Normal(3) = Vector3<float>{ 0, 0, -1 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 1.0f, 1.0f };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 0.0f, 1.0f };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 0.0f, 0.0f };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 1.0f, 0.0f };

                meshBuffer->Color(0, 0 + i) = c.ToArray();
                meshBuffer->Color(0, 1 + i) = c.ToArray();
                meshBuffer->Color(0, 2 + i) = c.ToArray();
                meshBuffer->Color(0, 3 + i) = c.ToArray();
            }

            // fill indices
            int vertices = 0;
            for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i += 2, vertices += 4)
            {
                meshBuffer->GetIndice()->SetTriangle(i,
                    (unsigned int)0 + vertices, (unsigned int)1 + vertices, (unsigned int)2 + vertices);
                meshBuffer->GetIndice()->SetTriangle(i + 1,
                    (unsigned int)2 + vertices, (unsigned int)3 + vertices, (unsigned int)0 + vertices);
            }

            if (mIsPlayer)
            {
                // Move minimal Y position to 0 (feet position)
				for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
					meshBuffer->Position(i)[1] += dy;
            }

			// Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
            //meshBuffer->GetMaterial()->SetFlag(MF_FOG_ENABLE, true);
            meshBuffer->GetMaterial()->mType = mMaterialType;
			meshBuffer->GetMaterial()->mTypeParam2 = mShaderId;

			if (mEnableShaders) 
            {
                meshBuffer->GetMaterial()->mEmissive = c.ToArray();
                meshBuffer->GetMaterial()->mShadingModel = ShadingModel::SM_BLINN;
                //meshBuffer->GetMaterial()->SetFlag(MF_NORMALIZE_NORMALS, true);
			}

            // Add to mesh
            mesh->AddMeshBuffer(meshBuffer);
		}
		RecalculateBoundingBox(mesh);

		mMeshNode = std::make_shared<ObjectMeshNode>(
			mId, mShaderId, mEnvironment, &pScene->GetPVWUpdater(), mesh);
		mTransformNode->AttachChild(mMeshNode);

		// Set it to use the materials of the meshbuffers directly.
		// This is needed for changing the texture in the future
		mMeshNode->SetReadOnlyMaterials(true);
	} 
    else if (mProp.visual == "cube") 
    {
		GrabTransformNode();
        std::shared_ptr<BaseMesh> mesh = CreateCubeMesh(Vector3<float>{BS, BS, BS});
		mMeshNode = std::make_shared<ObjectMeshNode>(
			mId, mShaderId, mEnvironment, &pScene->GetPVWUpdater(), mesh);
		pScene->AddSceneNode(mId, mTransformNode);

		mMeshNode->GetRelativeTransform().SetScale(mProp.visualSize);
        if (mProp.backfaceCulling)
            mMeshNode->GetMaterial(0)->mCullMode = RasterizerState::CULL_BACK;

		SetSceneNodeMaterial(mMeshNode);
	} 
    else if (mProp.visual == "mesh") 
    {
		GrabTransformNode();

        std::shared_ptr<ResHandle>& resHandle =
            ResCache::Get()->GetHandle(&BaseResource(ToWideString(mProp.mesh)));
        if (resHandle)
        {
            const std::shared_ptr<MeshResourceExtraData>& extra =
                std::static_pointer_cast<MeshResourceExtraData>(resHandle->GetExtra());

            std::shared_ptr<SkinnedMesh> mesh = std::dynamic_pointer_cast<SkinnedMesh>(extra->GetMesh());
            if (mesh)
            {
				mAnimatedMeshNode = std::make_shared<AnimatedObjectMeshNode>(
					mId, mShaderId, mEnvironment, &pScene->GetPVWUpdater(), mesh);
				mTransformNode->AttachChild(mAnimatedMeshNode);

                if (!CheckMeshNormals(mesh))
                {
                    LogInformation("GenericVisualActiveObject: recalculating normals for mesh " + mProp.mesh);
                    RecalculateNormals(mesh, true, false);
                }

                mAnimatedMeshNode->AnimateJoints(); // Needed for some animations
                mAnimatedMeshNode->GetRelativeTransform().SetScale(mProp.visualSize);

                // set vertex colors to ensure alpha is set
                SetMeshColor(mAnimatedMeshNode->GetMesh(), SColor(0xFFFFFFFF));

                SetAnimatedMeshColor(mAnimatedMeshNode, SColor(0xFFFFFFFF));

                SetSceneNodeMaterial(mAnimatedMeshNode);

                if (mProp.backfaceCulling)
                    mAnimatedMeshNode->GetMaterial(0)->mCullMode = RasterizerState::CULL_BACK;
            }
        }
        else LogError("GenericVisualActiveObject::AddToScene(): Could not load mesh " + mProp.mesh);
	} 
    else if (mProp.visual == "wielditem" || mProp.visual == "item") 
    {
		GrabTransformNode();
		ItemStack item;
		if (mProp.wieldItem.empty()) 
        {
			// Old format, only textures are specified.
			LogInformation("textures: " + std::to_string(mProp.textures.size()));
			if (!mProp.textures.empty()) 
            {
				LogInformation("textures[0]: " + mProp.textures[0]);
				item = ItemStack(mProp.textures[0], 1, 0, mEnvironment->GetItemManager());
			}
		} 
        else 
        {
			LogInformation("serialized form: " + mProp.wieldItem);
			item.Deserialize(mProp.wieldItem, mEnvironment->GetItemManager());
		}
		mWieldMeshNode = std::make_shared<WieldMeshNode>(
			mId, false, mEnvironment, &pScene->GetPVWUpdater());
		pScene->AddSceneNode(mId, mWieldMeshNode);
		mWieldMeshNode->CreateMesh();
		mWieldMeshNode->SetItem(item, (mProp.visual == "wielditem"));
		mWieldMeshNode->SetColor(SColor(0xFFFFFFFF));
		mWieldMeshNode->SetCullingMode(CullingMode::CULL_DYNAMIC);

		mWieldMeshNode->GetRelativeTransform().SetScale(
			mWieldMeshNode->GetRelativeTransform().GetScale() * mProp.visualSize / 2.0f);
		mWieldMeshNode->UpdateAbsoluteTransform();
	} 
    else 
    {
		LogInformation("GenericVisualActiveObject::AddToScene(): \"" +
            mProp.visual + "\" not supported");
	}

	/* don't update while punch texture modifier is active */
	if (mResetTexturesTimer < 0)
		UpdateTextures(mCurrentTextureModifier);

	std::shared_ptr<Node> node = GetSceneNode();
	if (node && mTransformNode)
	{
		node->DetachParent();
		node->AttachParent(mTransformNode);
	}

	UpdateNameTag();
	UpdateMarker();
	UpdateNodePosition();
	UpdateAnimation();
	UpdateBonePosition();
	UpdateAttachments();
	SetNodeLight(mLastLight);
	UpdateMeshCulling();
}

void GenericVisualActiveObject::UpdateLight(unsigned int dayNightRatio)
{
	if (mGlow < 0)
		return;

    unsigned char lightAtPos = 0;
	bool posOk = false;

	Vector3<short> pos[3] = { Vector3<short>::Zero(), Vector3<short>::Zero(), Vector3<short>::Zero() };
	unsigned short nPos = GetLightPosition(pos);
	for (unsigned short i = 0; i < nPos; i++) 
    {
		bool thisOk;
		MapNode mapNode = mEnvironment->GetMap()->GetNode(pos[i], &thisOk);
		if (thisOk) 
        {
            unsigned char thisLight = mapNode.GetLightBlend(dayNightRatio, mEnvironment->GetNodeManager());
			lightAtPos = std::max(lightAtPos, thisLight);
			posOk = true;
		}
	}
	if (!posOk)
		lightAtPos = BlendLight(dayNightRatio, LIGHT_SUN, 0);

    unsigned char light = DecodeLight((unsigned char)(lightAtPos + mGlow));
	if (light != mLastLight) 
    {
		mLastLight = light;
		SetNodeLight(light);
	}
}

void GenericVisualActiveObject::SetNodeLight(unsigned char light)
{
	SColor color(255, light, light, light);

	if (mProp.visual == "wielditem" || mProp.visual == "item") 
    {
		if (mWieldMeshNode)
			mWieldMeshNode->SetNodeLightColor(color);
		return;
	}

	if (mEnableShaders) 
    {
		if (mProp.visual == "upright_sprite") 
        {
			if (!mMeshNode)
				return;

			std::shared_ptr<BaseMesh> mesh = mMeshNode->GetMesh();
			for (unsigned int i = 0; i < mesh->GetMeshBufferCount(); ++i) 
            {
				std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(i);
				buf->GetMaterial()->mEmissive = SColorF(color).ToArray();
			}
		} 
        else 
        {
			std::shared_ptr<Node> node = GetSceneNode();
			if (!node)
				return;

			for (unsigned int i = 0; i < node->GetMaterialCount(); ++i) 
            {
				std::shared_ptr<Material> material = node->GetMaterial(i);
				material->mEmissive = SColorF(color).ToArray();
			}
		}
	} 
    else 
    {
		if (mMeshNode)
        {
			SetMeshColor(mMeshNode->GetMesh(), color);
		} 
        else if (mAnimatedMeshNode) 
        {
			SetAnimatedMeshColor(mAnimatedMeshNode, color);
		} 
        else if (mSpriteNode) 
        {
			mSpriteNode->SetColor(color);
		}
	}
}

unsigned short GenericVisualActiveObject::GetLightPosition(Vector3<short>* pos)
{
	const auto &box = mProp.collisionBox;
    Vector3<float> p = mPosition + box.mMinEdge * BS;
	(pos[0])[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
	(pos[0])[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
	(pos[0])[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

    p = mPosition + box.mMaxEdge * BS;
	(pos[1])[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
	(pos[1])[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
	(pos[1])[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

	// Skip center pos if it falls into the same node as Min or MaxEdge
	if (LengthSq(box.mMaxEdge - box.mMinEdge) < 3.0f)
		return 2;
    p = mPosition + box.GetCenter() * BS;
	(pos[2])[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
	(pos[2])[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
	(pos[2])[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);
	return 3;
}

void GenericVisualActiveObject::UpdateMarker()
{
	if (!mEnvironment->GetMinimap())
		return;

	if (!mProp.showOnMinimap) 
    {
		if (mMarker)
            mEnvironment->GetMinimap()->RemoveMarker(&mMarker);
		return;
	}

	if (mMarker)
		return;

	std::shared_ptr<Node> node = GetSceneNode();
	if (!node)
		return;
	mMarker = mEnvironment->GetMinimap()->AddMarker(node.get());
}

void GenericVisualActiveObject::UpdateNameTag()
{
	if (mIsVisualPlayer) // No nameTag for local player
		return;

	if (mProp.nameTag.empty() || mProp.nameTagColor.GetAlpha() == 0) 
    {
		// Delete nameTag
		if (mNameTag) 
        {
            mEnvironment->GetPlayerCamera()->RemoveNameTag(mNameTag);
			mNameTag = nullptr;
		}
		return;
	}

	std::shared_ptr<Node> node = GetSceneNode();
	if (!node)
		return;

	Vector3<float> pos;
	pos[1] = mProp.selectionBox.mMaxEdge[1] + 0.3f;
	if (!mNameTag) 
    {
		// Add nameTag
		mNameTag = mEnvironment->GetPlayerCamera()->AddNameTag(node.get(),
			mProp.nameTag, mProp.nameTagColor, mProp.nameTagBGColor, pos);
	} 
    else 
    {
		// Update nameTag
		mNameTag->text = mProp.nameTag;
		mNameTag->textcolor = mProp.nameTagColor;
		mNameTag->bgcolor = mProp.nameTagBGColor;
		mNameTag->pos = pos;
	}
}

void GenericVisualActiveObject::UpdateNodePosition()
{
	if (GetParent() != NULL)
		return;

	std::shared_ptr<Node> node = GetSceneNode();
	if (node) 
    {
		Vector3<short> cameraOffset = mEnvironment->GetCameraOffset();
		Vector3<float> pos = mPosTranslator.valCurrent - 
			Vector3<float>{(float)cameraOffset[0], (float)cameraOffset[1], (float)cameraOffset[2]} * BS;
        mTransformNode->GetRelativeTransform().SetTranslation(pos);

		if (node != mSpriteNode) 
        { 
            // rotate if not a sprite
			Vector3<float> rot = mIsVisualPlayer ? mRotation : mRotTranslator.valCurrent;
			float yaw = rot[1] * (float)GE_C_DEG_TO_RAD;
			float pitch = rot[2] * (float)GE_C_DEG_TO_RAD;
			float roll = rot[0] * (float)GE_C_DEG_TO_RAD;

			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), yaw));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), pitch));
			Matrix4x4<float> rollRotation = Rotation<4, float>(
				AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), roll));
            mTransformNode->GetRelativeTransform().SetRotation(
				yawRotation * pitchRotation * rollRotation);
		}
	}
}

void GenericVisualActiveObject::Step(float dTime, VisualEnvironment* env)
{
	// Handle model animations and update positions instantly to prevent lags
	if (mIsVisualPlayer) 
    {
		VisualPlayer* player = mEnvironment->GetPlayer();
		mPosition = player->GetPosition();
		mPosTranslator.valCurrent = mPosition;
		mRotation[1] = WrapDegrees360(player->GetYaw());
		mRotTranslator.valCurrent = mRotation;

		if (mIsVisible) 
        {
			int oldAnim = player->mLastAnimation;
			float oldAnimSpeed = player->mLastAnimationSpeed;
            mVelocity.MakeZero();
            mAcceleration.MakeZero();
			const PlayerControl& controls = player->GetPlayerControl();

			bool walking = false;
			if (controls.up || controls.down || controls.left || controls.right)
				walking = true;

			float newSpeed = player->mLocalAnimationSpeed;
			Vector2<short> newAnim = Vector2<short>::Zero();
			bool allowUpdate = false;

			// increase speed if using fast or flying fast
			if((Settings::Get()->GetBool("fast_move") && mEnvironment->CheckLocalPrivilege("fast")) &&
                (controls.aux1 || (!player->mTouchingGround && Settings::Get()->GetBool("free_move") &&
                    mEnvironment->CheckLocalPrivilege("fly"))))
					newSpeed *= 1.5;

			// slowdown speed if sneeking
			if (controls.sneak && walking)
				newSpeed /= 2;

			if (walking && (controls.dig || controls.place)) 
            {
				newAnim = player->mLocalAnimations[3];
				player->mLastAnimation = WD_ANIM;
			} 
            else if (walking) 
            {
				newAnim = player->mLocalAnimations[1];
				player->mLastAnimation = WALK_ANIM;
			} 
            else if (controls.dig || controls.place) 
            {
				newAnim = player->mLocalAnimations[2];
				player->mLastAnimation = DIG_ANIM;
			}

			// Apply animations if input detected and not attached
			// or set idle animation
			if ((newAnim[0] + newAnim[1]) > 0 && !GetParent()) 
            {
				allowUpdate = true;
				mAnimationRange = newAnim;
				mAnimationSpeed = newSpeed;
				player->mLastAnimationSpeed = mAnimationSpeed;
			} 
            else 
            {
				player->mLastAnimation = NO_ANIM;
				if (oldAnim != NO_ANIM)
                {
					mAnimationRange = player->mLocalAnimations[0];
					UpdateAnimation();
				}
			}

			// Update local player animations
			if ((player->mLastAnimation != oldAnim ||
                mAnimationSpeed != oldAnimSpeed) &&
                player->mLastAnimation != NO_ANIM && allowUpdate)
				UpdateAnimation();

		}
	}

	if (mVisualsExpired && GameApplication::Get()->GetHumanView()->mScene)
    {
		mVisualsExpired = false;

		// Attachments, part 1: All attached objects must be unparented first,
		// or game engine causes a segmentation fault
		for (unsigned short vaoId : mAttachmentChildIds) 
        {
			VisualActiveObject* obj = mEnvironment->GetActiveObject(vaoId);
			if (obj) 
            {
				std::shared_ptr<Node> childNode = obj->GetSceneNode();
				// The node's parent is always an IDummyTraformationSceneNode,
				// so we need to reparent that one instead.
                if (childNode)
                {
                    Node* parent = dynamic_cast<Node*>(childNode->GetParent());
                    parent->DetachParent();
                    parent->AttachParent(GameApplication::Get()->GetHumanView()->mScene->GetRootNode());
                }
			}
		}

		RemoveFromScene(false);
		AddToScene();

		// Attachments, part 2: Now that the parent has been refreshed, put its attachments back
		for (unsigned short vaoId : mAttachmentChildIds) 
        {
			VisualActiveObject* obj = mEnvironment->GetActiveObject(vaoId);
			if (obj)
				obj->UpdateAttachments();
		}
	}

	// Make sure mIsVisible is always applied
	std::shared_ptr<Node> node = GetSceneNode();
	if (node)
		node->SetVisible(mIsVisible);

	if(GetParent() != NULL) // Attachments should be glued to their parent by game engine
	{
		// Set these for later
		mPosition = GetPosition();
        mVelocity = Vector3<float>{ 0,0,0 };
        mAcceleration = Vector3<float>{ 0,0,0 };
		mPosTranslator.valCurrent = mPosition;
		mPosTranslator.valTarget = mPosition;
	} 
    else 
    {
		mRotTranslator.Translate(dTime);
		Vector3<float> lastpos = mPosTranslator.valCurrent;

		if(mProp.physical)
		{
			BoundingBox<float> box = mProp.collisionBox;
			box.mMinEdge *= BS;
			box.mMaxEdge *= BS;
			CollisionMoveResult moveResult;
			float posMaxDist = BS*0.125; // Distance per iteration
			Vector3<float> pPos = mPosition;
			Vector3<float> pVelocity = mVelocity;
            moveResult = CollisionMoveSimple(env, posMaxDist, box, mProp.stepHeight, 
				dTime, &pPos, &pVelocity, mAcceleration, this, mProp.collideWithObjects);
			// Apply results
			mPosition = pPos;
			mVelocity = pVelocity;

			bool isEndPosition = moveResult.collides;
			mPosTranslator.Update(mPosition, isEndPosition, dTime);
		} 
        else 
        {
			mPosition += dTime * mVelocity + 0.5f * dTime * dTime * mAcceleration;
			mVelocity += dTime * mAcceleration;
			mPosTranslator.Update(mPosition, mPosTranslator.aimIsEnd, mPosTranslator.animTime);
		}
		mPosTranslator.Translate(dTime);
		UpdateNodePosition();

		float moved = Length(lastpos - mPosTranslator.valCurrent);
		mStepDistanceCounter += moved;
		if (mStepDistanceCounter > 1.5f * BS) 
        {
			mStepDistanceCounter = 0.0f;
			if (!mIsVisualPlayer && mProp.makesFootstepSound) 
            {
                const NodeManager* nodeMgr = env->GetNodeManager();
                Vector3<float> p = GetPosition() +
                    Vector3<float>{0.0f, (mProp.collisionBox.mMinEdge[1] - 0.5f) * BS, 0.0f};
                Vector3<short> pp;
                pp[0] = (short)((p[0] + (p[0] > 0 ? BS / 2 : -BS / 2)) / BS);
                pp[1] = (short)((p[1] + (p[1] > 0 ? BS / 2 : -BS / 2)) / BS);
                pp[2] = (short)((p[2] + (p[2] > 0 ? BS / 2 : -BS / 2)) / BS);

				MapNode node = mEnvironment->GetMap()->GetNode(pp);
				SimpleSound spec = nodeMgr->Get(node).soundFootstep;
				// Reduce footstep gain, as non-local-player footsteps are
				// somehow louder.
				spec.gain *= 0.6f;
				mEnvironment->GetSoundManager()->PlaySoundAt(spec, false, GetPosition());
			}
		}
	}

	mAnimTimer += dTime;
	if(mAnimTimer >= mAnimFramelength)
	{
		mAnimTimer -= mAnimFramelength;
		mAnimFrame++;
		if(mAnimFrame >= mAnimNumFrames)
			mAnimFrame = 0;
	}

	UpdateTexturePosition();

	if(mResetTexturesTimer >= 0)
	{
		mResetTexturesTimer -= dTime;
		if(mResetTexturesTimer <= 0) 
        {
			mResetTexturesTimer = -1;
			UpdateTextures(mPreviousTextureModifier);
		}
	}

	if (!GetParent() && node && fabs(mProp.automaticRotate) > 0.001f) 
    {
		// This is the child node's rotation. It is only used for automaticRotate.
		AxisAngle<4, float> localRotation;
        node->GetRelativeTransform().GetRotation(localRotation);
		/*
		float yaw = (localRotation.mAngle - dTime * mProp.automaticRotate) * GE_C_RAD_TO_DEG;
        localRotation.mAngle = Modulo360(yaw) * (float)GE_C_DEG_TO_RAD;
		*/
		localRotation.mAngle *= localRotation.mAxis[AXIS_Y];
		localRotation.mAngle += dTime * mProp.automaticRotate;

		Matrix4x4<float> yawRotation = Rotation<4, float>(AxisAngle<4, float>(
			Vector4<float>::Unit(AXIS_Y), localRotation.mAngle));
        node->GetRelativeTransform().SetRotation(yawRotation);
		node->UpdateAbsoluteTransform();
	}

	if (!GetParent() && mProp.automaticFaceMovementDir &&
        (fabs(mVelocity[2]) > 0.001f || fabs(mVelocity[0]) > 0.001f)) 
    {
		float targetYaw = atan2(mVelocity[2], mVelocity[0]) * 180.f / (float)GE_C_PI + mProp.automaticFaceMovementDirOffset;
		float maxRotationPerSec = mProp.automaticFaceMovementMaxRotationPerSec;

		if (maxRotationPerSec > 0) 
        {
			WrappedApproachShortest(mRotation[1], targetYaw, dTime* maxRotationPerSec, 360.f);
		} 
        else 
        {
			// Negative values of maxRotationPerSec mean disabled.
			mRotation[1] = targetYaw;
		}

		mRotTranslator.valCurrent = mRotation;
		UpdateNodePosition();
	}

	if (mAnimatedMeshNode) 
    {
		// Everything must be updated; the whole transform
		// chain as well as the animated mesh node.
		// Otherwise, bone attachments would be relative to
		// a position that's one frame old.
		if (mTransformNode)
			UpdatePositionRecursive(mTransformNode.get());
		mAnimatedMeshNode->UpdateAbsoluteTransform();
		mAnimatedMeshNode->AnimateJoints();
		UpdateBonePosition();
	}
}

void GenericVisualActiveObject::UpdateTexturePosition()
{
	if(mSpriteNode)
	{
		std::shared_ptr<CameraNode> camera = GameApplication::Get()->GetHumanView()->mScene->GetActiveCamera();
		if(!camera)
			return;
		Vector3<float> camToEntity = 
            mSpriteNode->GetAbsoluteTransform().GetTranslation() -
            camera->GetAbsoluteTransform().GetTranslation();
		Normalize(camToEntity);

		int row = mTxBasePos[1];
		int col = mTxBasePos[0];

		// Yawpitch goes rightwards
		if (mTxSelectHorizontalByYawPitch) 
        {
			if (camToEntity[1] > 0.75)
				col += 5;
			else if (camToEntity[1] < -0.75)
				col += 4;
			else 
            {
				float mobDir = atan2(camToEntity[2], camToEntity[0]) / (float)GE_C_PI * 180.f;
				float dir = mobDir - mRotation[1];
				dir = WrapDegrees180(dir);
				if (std::fabs(WrapDegrees180(dir - 0)) <= 45.1f)
					col += 2;
				else if(std::fabs(WrapDegrees180(dir - 90)) <= 45.1f)
					col += 3;
				else if(std::fabs(WrapDegrees180(dir - 180)) <= 45.1f)
					col += 0;
				else if(std::fabs(WrapDegrees180(dir + 90)) <= 45.1f)
					col += 1;
				else
					col += 4;
			}
		}

		// Animation goes downwards
		row += mAnimFrame;

		float txs = mTxSize[0];
		float tys = mTxSize[1];
		SetBillboardTextureMatrix(mSpriteNode, txs, tys, col, row);
	}

	else if (mMeshNode) 
    {
		if (mProp.visual == "upright_sprite") 
        {
			float row = mTxBasePos[1];
            float col = mTxBasePos[0];

			// Animation goes downwards
			row += mAnimFrame;

			const auto &tx = mTxSize;
			Vector2<float> t[4] = { // cf. vertices in GenericVisualActiveObject::AddToScene()
                tx * Vector2<float>{col + 1, row + 1},
                tx * Vector2<float>{col, row + 1},
                tx * Vector2<float>{col, row},
                tx * Vector2<float>{col + 1, row},
			};
			auto mesh = mMeshNode->GetMesh();
			SetMeshBufferTextureCoords(mesh->GetMeshBuffer(0), t, 4);
			SetMeshBufferTextureCoords(mesh->GetMeshBuffer(1), t, 4);
		}
	}
}

// Do not pass by reference, see header.
void GenericVisualActiveObject::UpdateTextures(std::string mod)
{
    BaseTextureSource* texSrc = mEnvironment->GetTextureSource();

    bool useAnisotropicFilter = Settings::Get()->GetBool("anisotropic_filter");
    bool useBilinearFilter = Settings::Get()->GetBool("bilinear_filter");
    bool useTrilinearFilter = Settings::Get()->GetBool("trilinear_filter");

	mPreviousTextureModifier = mCurrentTextureModifier;
	mCurrentTextureModifier = mod;
	mGlow = mProp.glow;

	if (mSpriteNode) 
    {
		if (mProp.visual == "sprite") 
        {
			std::string textureString = "unknown_node.png";
			if (!mProp.textures.empty())
				textureString = mProp.textures[0];
			textureString += mod;
			mSpriteNode->GetMaterial(0)->mType = mMaterialType;
			mSpriteNode->GetMaterial(0)->mTypeParam = 0.5f;
			mSpriteNode->GetMaterial(0)->mTypeParam2 = mShaderId;
			mSpriteNode->SetMaterialTexture(0,
                texSrc->GetTextureForMesh(textureString));

			// This allows setting per-material colors. However, until a real lighting
			// system is added, the code below will have no effect. Once MineTest
			// has directional lighting, it should work automatically.
			if (!mProp.colors.empty()) 
            {
				mSpriteNode->GetMaterial(0)->mAmbient = SColorF(mProp.colors[0]).ToArray();
				mSpriteNode->GetMaterial(0)->mDiffuse = SColorF(mProp.colors[0]).ToArray();
				mSpriteNode->GetMaterial(0)->mSpecular = SColorF(mProp.colors[0]).ToArray();
			}

            //bilinear interpolation (no mipmapping)
            if (useBilinearFilter)
                mSpriteNode->GetMaterial(0)->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
            //trilinear interpolation (mipmapping)
            if (useTrilinearFilter)
                mSpriteNode->GetMaterial(0)->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
            if (useAnisotropicFilter)
                mSpriteNode->GetMaterial(0)->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
		}
	}
	else if (mAnimatedMeshNode)
    {
		if (mProp.visual == "mesh") 
        {
			for (unsigned int i = 0; i < mProp.textures.size() &&
                i < mAnimatedMeshNode->GetMaterialCount(); ++i) 
            {
				std::string textureString = mProp.textures[i];
				if (textureString.empty())
					continue; // Empty texture string means don't modify that material
				textureString += mod;
				std::shared_ptr<Texture2> texture = mEnvironment->GetTextureSource()->GetTextureForMesh(textureString);
				if (!texture) 
                {
					LogError("GenericVisualActiveObject::UpdateTextures(): Could not load texture " + textureString);
					continue;
				}

				// Set material flags and texture
				mAnimatedMeshNode->GetMaterial(i)->mType = mMaterialType;
				mAnimatedMeshNode->GetMaterial(i)->mTypeParam = 0.5f;
				mAnimatedMeshNode->GetMaterial(i)->mTypeParam2 = mShaderId;
				mAnimatedMeshNode->GetMaterial(i)->mTextureLayer[0].mTexture = texture;
				mAnimatedMeshNode->GetMaterial(i)->mLighting = true;
				//material->mTextureLayer[0].mFilter = none;
				mAnimatedMeshNode->GetMaterial(i)->mCullMode = mProp.backfaceCulling ? 
					RasterizerState::CULL_BACK : RasterizerState::CULL_NONE;

				// don't filter low-res textures, makes them look blurry
				// player models have a res of 64
				const Vector2<unsigned int>& size = mEnvironment->GetTextureSource()->GetTextureOriginalSize(textureString);
				const unsigned int res = std::min(size[1], size[0]);
				useTrilinearFilter &= res > 64;
				useBilinearFilter &= res > 64;

                //bilinear interpolation (no mipmapping)
                if (useBilinearFilter)
					mAnimatedMeshNode->GetMaterial(i)->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
                //trilinear interpolation (mipmapping)
                if (useTrilinearFilter)
					mAnimatedMeshNode->GetMaterial(i)->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
                if (useAnisotropicFilter)
					mAnimatedMeshNode->GetMaterial(i)->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
			}
			for (unsigned int i = 0; i < mProp.colors.size() && i < mAnimatedMeshNode->GetMaterialCount(); ++i)
			{
				// This allows setting per-material colors. However, until a real lighting
				// system is added, the code below will have no effect. Once MineTest
				// has directional lighting, it should work automatically.
				mAnimatedMeshNode->GetMaterial(i)->mAmbient = SColorF(mProp.colors[i]).ToArray();
				mAnimatedMeshNode->GetMaterial(i)->mDiffuse = SColorF(mProp.colors[i]).ToArray();
				mAnimatedMeshNode->GetMaterial(i)->mSpecular = SColorF(mProp.colors[i]).ToArray();
			}
		}
	}

	else if (mMeshNode) 
    {
		if(mProp.visual == "cube")
		{
			for (unsigned int i = 0; i < 6; ++i)
			{
				std::string textureString = "unknown_node.png";
				if(mProp.textures.size() > i)
					textureString = mProp.textures[i];
				textureString += mod;

				// Set material flags and texture
				mMeshNode->GetMaterial(i)->mType = mMaterialType;
				mMeshNode->GetMaterial(i)->mTypeParam = 0.5f;
				mMeshNode->GetMaterial(i)->mTypeParam2 = mShaderId;
				mMeshNode->GetMaterial(i)->mLighting = false;
                //material->mTextureLayer[0].mFilter = none;
				mMeshNode->GetMaterial(i)->SetTexture(0, texSrc->GetTextureForMesh(textureString));
				mMeshNode->GetMaterial(i)->GetTextureTransform(0).MakeIdentity();

				// This allows setting per-material colors. However, until a real lighting
				// system is added, the code below will have no effect. Once MineTest
				// has directional lighting, it should work automatically.
				if(mProp.colors.size() > i)
				{
					mMeshNode->GetMaterial(i)->mAmbient = SColorF(mProp.colors[i]).ToArray();
					mMeshNode->GetMaterial(i)->mDiffuse = SColorF(mProp.colors[i]).ToArray();
					mMeshNode->GetMaterial(i)->mSpecular = SColorF(mProp.colors[i]).ToArray();
				}

                //bilinear interpolation (no mipmapping)
                if (useBilinearFilter)
                    mMeshNode->GetMaterial(i)->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
                //trilinear interpolation (mipmapping)
                if (useTrilinearFilter)
                    mMeshNode->GetMaterial(i)->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
                if (useAnisotropicFilter)
                    mMeshNode->GetMaterial(i)->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
			}
		} 
        else if (mProp.visual == "upright_sprite") 
        {
			std::shared_ptr<BaseMesh> mesh = mMeshNode->GetMesh();
			{
				std::string tname = "unknown_object.png";
				if (!mProp.textures.empty())
					tname = mProp.textures[0];
				tname += mod;
				
                std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(0);
				buf->GetMaterial()->SetTexture(0, texSrc->GetTextureForMesh(tname));

				// This allows setting per-material colors. However, until a real lighting
				// system is added, the code below will have no effect. Once MineTest
				// has directional lighting, it should work automatically.
				if(!mProp.colors.empty()) 
                {
					buf->GetMaterial()->mAmbient = SColorF(mProp.colors[0]).ToArray();
					buf->GetMaterial()->mDiffuse = SColorF(mProp.colors[0]).ToArray();
					buf->GetMaterial()->mSpecular = SColorF(mProp.colors[0]).ToArray();
				}

                //bilinear interpolation (no mipmapping)
                if (useBilinearFilter)
                    buf->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
                //trilinear interpolation (mipmapping)
                if (useTrilinearFilter)
                    buf->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
                if (useAnisotropicFilter)
                    buf->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
			}

			{
				std::string tname = "unknown_object.png";
				if (mProp.textures.size() >= 2)
					tname = mProp.textures[1];
				else if (!mProp.textures.empty())
					tname = mProp.textures[0];
				tname += mod;
				
                std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(1);
				buf->GetMaterial()->SetTexture(0, texSrc->GetTextureForMesh(tname));

				// This allows setting per-material colors. However, until a real lighting
				// system is added, the code below will have no effect. Once MineTest
				// has directional lighting, it should work automatically.
				if (mProp.colors.size() >= 2) 
                {
					buf->GetMaterial()->mAmbient = SColorF(mProp.colors[1]).ToArray();
					buf->GetMaterial()->mDiffuse = SColorF(mProp.colors[1]).ToArray();
					buf->GetMaterial()->mSpecular = SColorF(mProp.colors[1]).ToArray();
				} 
                else if (!mProp.colors.empty()) 
                {
					buf->GetMaterial()->mAmbient = SColorF(mProp.colors[0]).ToArray();
					buf->GetMaterial()->mDiffuse = SColorF(mProp.colors[0]).ToArray();
					buf->GetMaterial()->mSpecular = SColorF(mProp.colors[0]).ToArray();
				}

                //bilinear interpolation (no mipmapping)
                if (useBilinearFilter)
                    buf->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
                //trilinear interpolation (mipmapping)
                if (useTrilinearFilter)
                    buf->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
                if (useAnisotropicFilter)
                    buf->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
			}
			// Set mesh color (only if lighting is disabled)
			if (!mProp.colors.empty() && mGlow < 0)
				SetMeshColor(mesh, mProp.colors[0]);
		}
	}
	// Prevent showing the player after changing texture
	if (mIsVisualPlayer)
		UpdateMeshCulling();
}

void GenericVisualActiveObject::UpdateAnimation()
{
	if (!mAnimatedMeshNode)
		return;

	if (mAnimatedMeshNode->GetStartFrame() != mAnimationRange[0] ||
		mAnimatedMeshNode->GetEndFrame() != mAnimationRange[1])
		mAnimatedMeshNode->SetFrameLoop(mAnimationRange[0], mAnimationRange[1]);
	if (mAnimatedMeshNode->GetAnimationSpeed() != mAnimationSpeed)
		mAnimatedMeshNode->SetAnimationSpeed(mAnimationSpeed);
	mAnimatedMeshNode->SetTransitionTime(mAnimationBlend);
	if (mAnimatedMeshNode->GetLoopMode() != mAnimationLoop)
		mAnimatedMeshNode->SetLoopMode(mAnimationLoop);
}

void GenericVisualActiveObject::UpdateAnimationSpeed()
{
	if (!mAnimatedMeshNode)
		return;

	mAnimatedMeshNode->SetAnimationSpeed(mAnimationSpeed);
}

void GenericVisualActiveObject::UpdateBonePosition()
{
	if (mBonePosition.empty() || !mAnimatedMeshNode)
		return;

	mAnimatedMeshNode->SetJointMode(JAUOR_CONTROL); // To write positions to the mesh on render
	for (auto& it : mBonePosition) 
    {
		std::string boneName = it.first;
		std::shared_ptr<BoneNode> bone = mAnimatedMeshNode->GetJointNode(boneName.c_str());
		if (bone) 
        {
			bone->GetRelativeTransform().SetTranslation(it.second[0]);

            Vector3<float>& rot = it.second[1];
			float yaw = rot[1] * (float)GE_C_DEG_TO_RAD;
			float pitch = rot[2] * (float)GE_C_DEG_TO_RAD;
			float roll = rot[0] * (float)GE_C_DEG_TO_RAD;

			Matrix4x4<float> yawRotation = Rotation<4, float>(
				AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Y), yaw));
			Matrix4x4<float> pitchRotation = Rotation<4, float>(
				AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_X), pitch));
			Matrix4x4<float> rollRotation = Rotation<4, float>(
				AxisAngle<4, float>(-Vector4<float>::Unit(AXIS_Z), roll));
			bone->GetRelativeTransform().SetRotation(
				yawRotation * pitchRotation * rollRotation);
			bone->UpdateAbsoluteTransform();
		}
	}

	// search through bones to find mistakenly rotated bones due to bug in game engine
	for (unsigned int i = 0; i < mAnimatedMeshNode->GetJointCount(); ++i)
    {
		std::shared_ptr<BoneNode> bone = mAnimatedMeshNode->GetJointNode(i);
		if (!bone)
			continue;

		//If bone is manually positioned there is no need to perform the bug check
		bool skip = false;
		for (auto& it : mBonePosition) 
        {
			if (it.first == bone->GetName()) 
            {
				skip = true;
				break;
			}
		}
		if (skip) continue;

		// Workaround for game engine bug
		// We check each bone to see if it has been rotated ~180deg from its expected position due to a bug in Irricht
		// when using EJUOR_CONTROL joint control. If the bug is detected we update the bone to the proper position
		// and update the bones transformation.
        /*EulerAngles<float> boneRot;
        bone->GetRelativeTransform().GetRotation(boneRot);
		float offset = fabsf(boneRot[0] - bone->GetRotation()[0]);
		if (offset > 179.9f && offset < 180.1f) 
        {
			bone->GetRelativeTransform().SetRotation(boneRot);
			bone->UpdateAbsoluteTransform();
		}*/
	}
	// The following is needed for set_bone_pos to propagate to
	// attached objects correctly.
	// Game engine ought to do this, but doesn't when using EJUOR_CONTROL.
	for (unsigned int i = 0; i < mAnimatedMeshNode->GetJointCount(); ++i)
    {
		auto bone = mAnimatedMeshNode->GetJointNode(i);
		// Look for the root bone.
		if (bone && bone->GetParent() == mAnimatedMeshNode.get())
        {
			// Update entire skeleton.
			bone->UpdateAbsoluteTransformationChildren();
			break;
		}
	}
}

void GenericVisualActiveObject::UpdateAttachments()
{
	VisualActiveObject* parent = GetParent();

	mAttachedToLocal = parent && parent->IsVisualPlayer();

	/*
	Following cases exist:
		mAttachmentParentId == 0 && !parent
			This object is not attached
		mAttachmentParentId != 0 && parent
			This object is attached
		mAttachmentParentId != 0 && !parent
			This object will be attached as soon the parent is known
		mAttachmentParentId == 0 && parent
			Impossible case
	*/

	if (!parent) 
    { 
        // Detach or don't attach
		if (mTransformNode) 
        {
			Vector3<short> cameraOffset = mEnvironment->GetCameraOffset();
			Vector3<float> oldPos = GetPosition();

			mTransformNode->DetachParent();
			mTransformNode->AttachParent(GameApplication::Get()->GetHumanView()->mScene->GetRootNode());

            GetRelativeTransform().SetTranslation(oldPos - Vector3<float>{
                cameraOffset[0] * BS, cameraOffset[1] * BS, cameraOffset[2] * BS});
            mTransformNode->UpdateAbsoluteTransform();
		}
	}
	else // Attach
	{
		parent->UpdateAttachments();
		std::shared_ptr<Node> parentNode = parent->GetSceneNode();
		std::shared_ptr<AnimatedObjectMeshNode> parentAnimatedObjectMeshNode = parent->GetAnimatedMeshSceneNode();
		if (parentAnimatedObjectMeshNode && !mAttachmentBone.empty()) 
			parentNode = parentAnimatedObjectMeshNode->GetJointNode(mAttachmentBone.c_str());

		if (mTransformNode && parentNode) 
        {
			mTransformNode->DetachParent();
			mTransformNode->AttachParent(parentNode);
			parentNode->UpdateAbsoluteTransform();
			GetRelativeTransform().SetTranslation(mAttachmentPosition);
			//SetPitchYawRoll(GetRelativeTransform(), mAttachmentRotation);
			// use game engine eulers instead
			GetRelativeTransform().SetRotation(mAttachmentRotation);
            mTransformNode->UpdateAbsoluteTransform();
		}
	}
}

bool GenericVisualActiveObject::VisualExpiryRequired(const ObjectProperties& newProp) const
{
	const ObjectProperties& old = mProp;
	/* Visuals do not need to be expired for:
	 * - nameTag props: handled by UpdateNameTag()
	 * - textures:      handled by UpdateTextures()
	 * - sprite props:  handled by UpdateTexturePosition()
	 * - glow:          handled by UpdateLight()
	 * - any other properties that do not change appearance
	 */

	bool usesLegacyTexture = newProp.wieldItem.empty() &&
		(newProp.visual == "wielditem" || newProp.visual == "item");
	// Ordered to compare primitive types before std::vectors
	return old.backfaceCulling != newProp.backfaceCulling ||
		old.isVisible != newProp.isVisible ||
		old.mesh != newProp.mesh ||
		old.shaded != newProp.shaded ||
		old.useTextureAlpha != newProp.useTextureAlpha ||
		old.visual != newProp.visual ||
		old.visualSize != newProp.visualSize ||
		old.wieldItem != newProp.wieldItem ||
		old.colors != newProp.colors ||
		(usesLegacyTexture && old.textures != newProp.textures);
}

void GenericVisualActiveObject::ProcessMessage(const std::string& data)
{
	//infostream<<"GenericVisualActiveObject: Got message"<<std::endl;
	std::istringstream is(data, std::ios::binary);
	// command
	uint8_t cmd = ReadUInt8(is);
	if (cmd == AO_CMD_SET_PROPERTIES) 
    {
		ObjectProperties newProp;
		newProp.showOnMinimap = mIsPlayer; // default

		newProp.Deserialize(is);

		// Check what exactly changed
		bool expireVisuals = VisualExpiryRequired(newProp);
		bool texturesChanged = mProp.textures != newProp.textures;

		// Apply changes
		mProp = std::move(newProp);

		mSelectionBox = mProp.selectionBox;
		mSelectionBox.mMinEdge *= BS;
		mSelectionBox.mMaxEdge *= BS;

		mTxSize[0] = 1.0f / mProp.spriteDiv[0];
		mTxSize[1] = 1.0f / mProp.spriteDiv[1];

		if(!mInitialTxBasePosSet)
        {
			mInitialTxBasePosSet = true;
			mTxBasePos = mProp.initialSpriteBasePos;
		}
		if (mIsVisualPlayer) 
        {
			VisualPlayer* player = mEnvironment->GetPlayer();
			player->mMakesFootstepSound = mProp.makesFootstepSound;
			BoundingBox<float> collisionBox = mProp.collisionBox;
            collisionBox.mMinEdge *= BS;
            collisionBox.mMaxEdge *= BS;
			player->GetCollisionBox() = collisionBox;
			player->SetEyeHeight(mProp.eyeHeight);
			player->SetZoomFOV(mProp.zoomFov);
		}

		if ((mIsPlayer && !mIsVisualPlayer) && mProp.nameTag.empty())
			mProp.nameTag = mName;
		if (mIsVisualPlayer)
			mProp.showOnMinimap = false;

		if (expireVisuals) 
        {
			ExpireVisuals();
		} 
        else 
        {
			LogInformation("GenericVisualActiveObject: properties updated but expiring visuals not necessary");
			if (texturesChanged) 
            {
				// don't update while punch texture modifier is active
				if (mResetTexturesTimer < 0)
					UpdateTextures(mCurrentTextureModifier);
			}
			UpdateNameTag();
			UpdateMarker();
		}
	} 
    else if (cmd == AO_CMD_UPDATE_POSITION) 
    {
		// Not sent by the logic if this object is an attachment.
		// We might however get here if the logic notices the object being detached before the visual.
		mPosition = ReadV3Float(is);
		mVelocity = ReadV3Float(is);
		mAcceleration = ReadV3Float(is);
		mRotation = ReadV3Float(is);

		mRotation = WrapDegrees360(mRotation);
		bool doInterpolate = ReadUInt8(is);
		bool isEndPosition = ReadUInt8(is);
		float updateInterval = ReadFloat(is);

		// Place us a bit higher if we're physical, to not sink into
		// the ground due to sucky collision detection...
		if(mProp.physical)
            mPosition += Vector3<float>{0, 0.002f, 0};

		if(GetParent() != NULL) // Just in case
			return;

		if(doInterpolate)
		{
			if(!mProp.physical)
				mPosTranslator.Update(mPosition, isEndPosition, updateInterval);
		} 
        else 
        {
			mPosTranslator.Init(mPosition);
		}
		mRotTranslator.Update(mRotation, false, updateInterval);
		UpdateNodePosition();
	} 
    else if (cmd == AO_CMD_SET_TEXTURE_MOD) 
    {
		std::string mod = DeserializeString16(is);

		// immediately reset a engine issued texture modifier if a mod sends a different one
		if (mResetTexturesTimer > 0) 
        {
			mResetTexturesTimer = -1;
			UpdateTextures(mPreviousTextureModifier);
		}
		UpdateTextures(mod);
	} 
    else if (cmd == AO_CMD_SET_SPRITE) 
    {
		Vector2<short> p = ReadV2Short(is);
		int numFrames = ReadUInt16(is);
		float frameLength = ReadFloat(is);
		bool selectHorizontalByYawPitch = ReadUInt8(is);

		mTxBasePos = p;
		mAnimNumFrames = numFrames;
		mAnimFramelength = frameLength;
		mTxSelectHorizontalByYawPitch = selectHorizontalByYawPitch;

		UpdateTexturePosition();
	} 
    else if (cmd == AO_CMD_SET_PHYSICS_OVERRIDE) 
    {
		float overrideSpeed = ReadFloat(is);
		float overrideJump = ReadFloat(is);
		float overrideGravity = ReadFloat(is);
		// these are sent inverted so we get true when the logic sends nothing
		bool sneak = !ReadUInt8(is);
		bool sneakGlitch = !ReadUInt8(is);
		bool newMove = !ReadUInt8(is);


		if(mIsVisualPlayer)
		{
			VisualPlayer* player = mEnvironment->GetPlayer();
			player->mPhysicsOverrideSpeed = overrideSpeed;
			player->mPhysicsOverrideJump = overrideJump;
			player->mPhysicsOverrideGravity = overrideGravity;
			player->mPhysicsOverrideSneak = sneak;
			player->mPhysicsOverrideSneakGlitch = sneakGlitch;
			player->mPhysicsOverrideNewMove = newMove;
		}
	} 
    else if (cmd == AO_CMD_SET_ANIMATION) 
    {
		// TODO: change frames send as Vector2<int> value
		Vector2<float> range = ReadV2Float(is);
		if (!mIsVisualPlayer) 
        {
            mAnimationRange = Vector2<short>{ (short)range[0], (short)range[1] };
			mAnimationSpeed = ReadFloat(is);
			mAnimationBlend = ReadFloat(is);
			// these are sent inverted so we get true when the logic sends nothing
			mAnimationLoop = !ReadUInt8(is);
			UpdateAnimation();
		} 
        else 
        {
			VisualPlayer* player = mEnvironment->GetPlayer();
			if(player->mLastAnimation == NO_ANIM)
			{
                mAnimationRange = Vector2<short>{ (short)range[0], (short)range[1] };
				mAnimationSpeed = ReadFloat(is);
				mAnimationBlend = ReadFloat(is);
				// these are sent inverted so we get true when the logic sends nothing
				mAnimationLoop = !ReadUInt8(is);
			}
			// update animation only if local animations present
			// and received animation is unknown (except idle animation)
			bool isKnown = false;
			for (int i = 1;i<4;i++)
				if(mAnimationRange[1] == player->mLocalAnimations[i][1])
					isKnown = true;

			if(!isKnown || (player->mLocalAnimations[1][1] + player->mLocalAnimations[2][1] < 1))
                UpdateAnimation();
		}
	} 
    else if (cmd == AO_CMD_SET_ANIMATION_SPEED) 
    {
		mAnimationSpeed = ReadFloat(is);
		UpdateAnimationSpeed();
	} 
    else if (cmd == AO_CMD_SET_BONE_POSITION) 
    {
		std::string bone = DeserializeString16(is);
		Vector3<float> position = ReadV3Float(is);
		Vector3<float> rotation = ReadV3Float(is);
        mBonePosition[bone] = Vector2<Vector3<float>>{ position, rotation };

		// UpdateBonePosition(); now called every step
	} 
    else if (cmd == AO_CMD_ATTACH_TO) 
    {
		unsigned short parentId = ReadInt16(is);
		std::string bone = DeserializeString16(is);
		Vector3<float> position = ReadV3Float(is);
		Vector3<float> rot = ReadV3Float(is);
		bool forceVisible = ReadUInt8(is); // Returns false for EOF

		float yaw = rot[1] * (float)GE_C_DEG_TO_RAD;
		float pitch = rot[2] * (float)GE_C_DEG_TO_RAD;
		float roll = rot[0] * (float)GE_C_DEG_TO_RAD;

		EulerAngles<float> yawPitchRoll;
		yawPitchRoll.mAxis[1] = 1;
		yawPitchRoll.mAxis[2] = 2;
		yawPitchRoll.mAngle[0] = roll;
		yawPitchRoll.mAngle[1] = yaw;
		yawPitchRoll.mAngle[2] = pitch;

		SetAttachment(parentId, bone, position, yawPitchRoll, forceVisible);
	} 
    else if (cmd == AO_CMD_PUNCHED) 
    {
		unsigned short resultHP = ReadUInt16(is);

		// Use this instead of the send damage to not interfere with prediction
		int damage = (int)mHP - (int)resultHP;

		mHP = resultHP;

		if (mIsVisualPlayer)
			mEnvironment->GetPlayer()->mHp = mHP;

		if (damage > 0)
		{
			if (mHP == 0)
			{
				// TODO: Execute defined fast response
				// As there is no definition, make a smoke puff
				VisualSimpleObject* simple = CreateSmokePuff(
					GameApplication::Get()->GetHumanView()->mScene.get(),
					mEnvironment, mPosition, Vector2<float>{mProp.visualSize[0], mProp.visualSize[1]} * BS);
				mEnvironment->AddSimpleObject(simple);
			} 
            else if (mResetTexturesTimer < 0 && !mProp.damageTextureModifier.empty()) 
            {
				mResetTexturesTimer = 0.05f;
				if(damage >= 2)
					mResetTexturesTimer += 0.05f * damage;
				UpdateTextures(mCurrentTextureModifier + mProp.damageTextureModifier);
			}
		}

		if (mHP == 0) 
        {
			// Same as 'DiePlayer'
			ClearParentAttachment();
			// Same as 'ObjectRef::l_remove'
			if (!mIsPlayer)
				ClearChildAttachments();
		}
	} 
    else if (cmd == AO_CMD_UPDATE_ARMOR_GROUPS) 
    {
		mArmorGroups.clear();
		int armorGroupsSize = ReadUInt16(is);
		for(int i=0; i<armorGroupsSize; i++)
		{
			std::string name = DeserializeString16(is);
			int rating = ReadInt16(is);
			mArmorGroups[name] = rating;
		}
	} 
    else if (cmd == AO_CMD_SPAWN_INFANT) 
    {
		unsigned short childId = ReadUInt16(is);
		uint8_t type = ReadUInt8(is); // maybe this will be useful later
		(void)type;

		AddAttachmentChild(childId);
	} 
    else if (cmd == AO_CMD_OBSOLETE1) 
    {
		// Don't do anything and also don't log a warning
	} 
    else 
    {
		LogWarning(FUNCTION_NAME ": unknown command or outdated visual \"" + 
            std::to_string(cmd) + "\"");
	}
}

/* \pre punchItem != NULL
 */
bool GenericVisualActiveObject::DirectReportPunch(Vector3<float> dir, 
    const ItemStack* punchItem, float timeFromLastPunch)
{
	LogAssert(punchItem, "invalid");	// pre-condition
	const ToolCapabilities* toolcap = 
		&punchItem->GetToolCapabilities(mEnvironment->GetItemManager());
	PunchDamageResult result = 
		GetPunchDamage(mArmorGroups, toolcap, punchItem, timeFromLastPunch);

	if(result.didPunch && result.damage != 0)
	{
		if(result.damage < mHP)
		{
			mHP -= result.damage;
		} 
        else 
        {
			mHP = 0;
			// TODO: Execute defined fast response
			// As there is no definition, make a smoke puff
			VisualSimpleObject* simple = CreateSmokePuff(
				GameApplication::Get()->GetHumanView()->mScene.get(),
				mEnvironment, mPosition, Vector2<float>{mProp.visualSize[0], mProp.visualSize[1]} * BS);
			mEnvironment->AddSimpleObject(simple);
		}
		if (mResetTexturesTimer < 0 && !mProp.damageTextureModifier.empty()) 
        {
			mResetTexturesTimer = 0.05f;
			if (result.damage >= 2)
				mResetTexturesTimer += 0.05f * result.damage;
			UpdateTextures(mCurrentTextureModifier + mProp.damageTextureModifier);
		}
	}

	return false;
}

std::string GenericVisualActiveObject::DebugInfoText()
{
	std::ostringstream os(std::ios::binary);
	os<<"GenericVisualActiveObject hp="<<mHP<<"\n";
	os<<"armor={";
	for(ItemGroupList::const_iterator i = mArmorGroups.begin(); i != mArmorGroups.end(); ++i)
		os << i->first.c_str() << "=" << i->second << ", ";
	os<<"}";
	return os.str();
}

void GenericVisualActiveObject::UpdateMeshCulling()
{
	if (!mIsVisualPlayer)
		return;

	const bool hidden = mEnvironment->GetPlayerCamera()->GetCameraMode() == CAMERA_MODE_FIRST;
	if (mMeshNode && mProp.visual == "upright_sprite") 
    {
		unsigned int buffers = (unsigned int)mMeshNode->GetMesh()->GetMeshBufferCount();
		for (unsigned int i = 0; i < buffers; i++) 
        {
			std::shared_ptr<Material> mat = mMeshNode->GetMesh()->GetMeshBuffer(i)->GetMaterial();
			// upright sprite has no backface culling
			mat->mCullMode = hidden ? RasterizerState::CULL_FRONT : RasterizerState::CULL_NONE;
		}
		return;
	}

	std::shared_ptr<Node> node = GetSceneNode();
	if (!node)
		return;

	if (hidden) 
    {
		// Hide the mesh by culling both front and
		// back faces. This also preserves the skeletal armature.
		node->SetCullingMode(CullingMode::CULL_ALWAYS);
	} 
    else 
    {
		// Restore mesh visibility.
		node->SetCullingMode(CullingMode::CULL_DYNAMIC);
		node->GetMaterial(0)->mCullMode = mProp.backfaceCulling ? 
			RasterizerState::CULL_BACK : RasterizerState::CULL_NONE;
	}
}

// Prototype
GenericVisualActiveObject ProtoGenericVAO(NULL);