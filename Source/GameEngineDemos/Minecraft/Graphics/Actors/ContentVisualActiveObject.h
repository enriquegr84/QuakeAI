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

#ifndef CONTENTVISUALACTIVEOBJECT_H
#define CONTENTVISUALACTIVEOBJECT_H

#include "GameEngineStd.h"

#include "../../Games/Actors/Item.h"
#include "../../Games/Actors/VisualObject.h"
#include "../../Games/Actors/ObjectProperties.h"

#include "Graphic/Scene/Scene.h"

#include "Mathematic/Algebra/Vector3.h"

class WieldMeshNode;
class ObjectMeshNode;
class AnimatedObjectMeshNode;
class BaseDummyTransformationNode;
class PlayerCamera;

struct Nametag;
struct MinimapMarker;

/*
	SmoothTranslator
*/

template<typename T>
struct SmoothTranslator
{
	T valOld;
	T valCurrent;
	T valTarget;
	float animTime = 0;
	float animTimeCounter = 0;
	bool aimIsEnd = true;

	SmoothTranslator() = default;

	void Init(T current);

	void Update(T newTarget, bool isEndPosition = false, float updateInterval = -1);

	void Translate(float dTime);
};

struct SmoothTranslatorWrapped : SmoothTranslator<float>
{
	void Translate(float dTime);
};

struct SmoothTranslatorWrappedVector3Float : SmoothTranslator<Vector3<float>>
{
	void Translate(float dTime);
};

class GenericVisualActiveObject : public VisualActiveObject
{
public:
	GenericVisualActiveObject(VisualEnvironment* env);

	~GenericVisualActiveObject();

	static VisualActiveObject* Create(VisualEnvironment* env)
	{
		return new GenericVisualActiveObject(env);
	}

	inline ActiveObjectType GetType() const
	{
		return ACTIVEOBJECT_TYPE_GENERIC;
	}
	inline const ItemGroupList& GetGroups() const
	{
		return mArmorGroups;
	}
	virtual void Initialize(const std::string& data);

	void ProcessInitData(const std::string& data);

    virtual bool GetCollisionBox(BoundingBox<float>* toset) const;
	virtual bool GetSelectionBox(BoundingBox<float>* toset) const;
    virtual bool CollideWithObjects() const;
	virtual const Vector3<float> GetPosition() const;
    virtual std::shared_ptr<Node> GetSceneNode() const;
    virtual std::shared_ptr<AnimatedObjectMeshNode> GetAnimatedMeshSceneNode() const;

	void SetPosition(const Vector3<float>& pos)
	{
		mPosTranslator.valCurrent = pos;
	}

	inline const Vector3<float>& GetRotation() const { return mRotation; }

	const bool IsImmortal();

	inline const ObjectProperties& GetProperties() const { return mProp; }

	// mMatrixNode controls the position and rotation of the child node
	// for all scene nodes, as a workaround for an Game engine problem with
	// rotations. The child node's position can't be used because it's
	// rotated, and must remain as 0.
	// Note that mMatrixNode.SetPosition() shouldn't be called. Use
	// mMatrixNode->GetRelativeTransformationMatrix().setTranslation()
	// instead (aka GetRelativeTransform().setTranslation()).
    Transform& GetRelativeTransform();
    Transform* GetAbsoluteTransform() const;

	inline float GetStepHeight() const
	{
		return mProp.stepHeight;
	}

	virtual bool IsVisualPlayer() const
	{
		return mIsVisualPlayer;
	}

	inline bool IsVisible() const
	{
		return mIsVisible;
	}

	inline void SetVisible(bool toset)
	{
		mIsVisible = toset;
	}

	void SetChildrenVisible(bool toset);
	void SetAttachment(int parentId, const std::string& bone, Vector3<float> position,
        EulerAngles<float> rotation, bool forceVisible);
	void GetAttachment(int* parentId, std::string* bone, Vector3<float>* position,
			EulerAngles<float>* rotation, bool* forceVisible) const;
	void ClearChildAttachments();
	void ClearParentAttachment();
	void AddAttachmentChild(int childId);
	void RemoveAttachmentChild(int childId);
	virtual VisualActiveObject* GetParent() const;
	virtual const std::unordered_set<int>& GetAttachmentChildIds() const { return mAttachmentChildIds; }
	virtual void UpdateAttachments();

	void RemoveFromScene(bool permanent);

	virtual void AddToScene();

	inline void ExpireVisuals() { mVisualsExpired = true; }

	void UpdateLight(unsigned int dayNightRatio);

	void SetNodeLight(unsigned char light);

	/* Get light position(s).
	 * returns number of positions written into pos[], which must have space
	 * for at least 3 vectors. */
	unsigned short GetLightPosition(Vector3<short>* pos);

	void UpdateNameTag();

	void UpdateMarker();

	void UpdateNodePosition();

	void Step(float dTime, VisualEnvironment* env);

	void UpdateTexturePosition();

	// ffs this HAS TO BE a string copy! See #5739 if you think otherwise
	// Reason: UpdateTextures(mPreviousTextureModifier);
	void UpdateTextures(std::string mod);

	void UpdateAnimation();

	void UpdateAnimationSpeed();

	void UpdateBonePosition();

	virtual void ProcessMessage(const std::string& data);

	virtual bool DirectReportPunch(Vector3<float> dir, 
        const ItemStack* punchItem=NULL, float timeFromLastPunch=1000000);

	std::string DebugInfoText();

    std::string InfoText() { return mProp.infoText; }

	void UpdateMeshCulling();

private:
    // Only set at initialization
    std::string mName = "";
    bool mIsPlayer = false;
    bool mIsVisualPlayer = false;
    // Property-ish things
    ObjectProperties mProp;
    //
    BoundingBox<float> mSelectionBox = BoundingBox<float>(
        -BS / 3.f, -BS / 3.f, -BS / 3.f, BS / 3.f, BS / 3.f, BS / 3.f);

    std::shared_ptr<BaseDummyTransformationNode> mTransformNode = nullptr;
    std::shared_ptr<AnimatedObjectMeshNode> mAnimatedMeshNode = nullptr;
    std::shared_ptr<WieldMeshNode> mWieldMeshNode = nullptr;
    std::shared_ptr<ObjectMeshNode> mMeshNode = nullptr;
    std::shared_ptr<BillboardNode> mSpriteNode = nullptr;

    Nametag* mNameTag = nullptr;
    MinimapMarker* mMarker = nullptr;
    Vector3<float> mPosition = Vector3<float>{ 0.0f, 10.0f * BS, 0 };
    Vector3<float> mVelocity = Vector3<float>::Zero();
    Vector3<float> mAcceleration = Vector3<float>::Zero();
    Vector3<float> mRotation = Vector3<float>::Zero();
    unsigned short mHP = 1;
    SmoothTranslator<Vector3<float>> mPosTranslator;
    SmoothTranslatorWrappedVector3Float mRotTranslator;
    // Spritesheet/animation stuff
    Vector2<float> mTxSize = Vector2<float>{ 1,1 };
    Vector2<short> mTxBasePos = Vector2<short>::Zero();
    bool mInitialTxBasePosSet = false;
    bool mTxSelectHorizontalByYawPitch = false;
    Vector2<short> mAnimationRange;
    float mAnimationSpeed = 15.0f;
    float mAnimationBlend = 0.0f;
    bool mAnimationLoop = true;
    // stores position and rotation for each bone name
    std::unordered_map<std::string, Vector2<Vector3<float>>> mBonePosition;

    int mAttachmentParentId = 0;
    std::unordered_set<int> mAttachmentChildIds;
    std::string mAttachmentBone = "";
    Vector3<float> mAttachmentPosition = Vector3<float>::Zero();
    EulerAngles<float> mAttachmentRotation;
    bool mAttachedToLocal = false;
    bool mForceVisible = false;

    int mAnimFrame = 0;
    int mAnimNumFrames = 1;
    float mAnimFramelength = 0.2f;
    float mAnimTimer = 0.0f;
    ItemGroupList mArmorGroups;
    float mResetTexturesTimer = -1.0f;
    // stores texture modifier before punch update
    std::string mPreviousTextureModifier = "";
    // last applied texture modifier
    std::string mCurrentTextureModifier = "";
    bool mVisualsExpired = false;
    float mStepDistanceCounter = 0.0f;
    unsigned char mLastLight = 255;
    bool mIsVisible = false;
    char mGlow = 0;
    // Material
    MaterialType mMaterialType;
    // Settings
    bool mEnableShaders = false;
	unsigned int mShaderId = 0;

    bool VisualExpiryRequired(const ObjectProperties& newS) const;
};

#endif