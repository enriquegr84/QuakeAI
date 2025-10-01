// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef CLOUDSYSTEMNODE_H
#define CLOUDSYSTEMNODE_H

#include "Graphic/Scene/Hierarchy/BoundingBox.h"

#include "Graphic/Effect/ColorEffect.h"
#include "Graphic/Scene/Hierarchy/Node.h"

#include "Graphic/Resource/Color.h"

#include "ShadowVolumeNode.h"

class CloudSystemNode : public Node
{
public:

    //! constructor
    CloudSystemNode(const ActorId actorId, PVWUpdater* updater);

    virtual ~CloudSystemNode();

    //! Renders event
    virtual bool PreRender(Scene* pScene);
    virtual bool Render(Scene* pScene);

    /** \return Axis aligned bounding box of this buffer. */
    virtual BoundingBox<float>& GetBoundingBox();

    //! Set effect for rendering.
    void SetEffect(unsigned short radius, bool enable3D);

    //! Returns type of the scene node
    virtual NodeType GetType() const { return NT_CLOUD; }

    //! Removes a child from this scene node.
    //! Implemented here, to be able to remove the shadow properly, if there is one,
    //! or to remove attached childs.
    virtual int DetachChild(std::shared_ptr<Node> const& child);

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

    //! Sets the material type of all materials in this scene node to a new material type.
    /** \param newType New type of material to be set. */
    virtual void SetMaterialType(MaterialType newType);

    //! Cloud step function
    void Update(float deltaMs);

    //! Update clouds.
    void Update(const Vector3<float>& cameraPos, SColorF color);

    void UpdateCameraOffset(const Vector3<short>& cameraOffset)
    {
        mCameraOffset = cameraOffset;
        UpdateBox();
    }

    void SetDensity(float density)
    {
        mDensity = density;
        // currently does not need bounding
    }

    void SetColorBright(const SColor& colorBright)
    {
        mColorBright = colorBright;
    }

    void SetColorAmbient(const SColor& colorAmbient)
    {
        mColorAmbient = colorAmbient;
    }

    void SetHeight(float height)
    {
        mHeight = height; // add bounding when necessary
        UpdateBox();
    }

    void SetSpeed(Vector2<float> speed)
    {
        mSpeed = speed;
    }

    void SetThickness(float thickness)
    {
        mThickness = thickness;
        UpdateBox();
    }

    bool IsCameraInsideCloud() const { return mCameraInsideCloud; }

    const SColor GetColor() const { return mColorDiffuse.ToSColor(); }

protected:

    //! Remove all currently visible cloud 
    void ClearClouds() { };

private:

    void UpdateBuffers();

    void UpdateBox();

    bool GridFilled(int x, int y) const;

    std::shared_ptr<BlendState> mBlendState;
    std::shared_ptr<DepthStencilState> mDepthStencilState;
    std::shared_ptr<RasterizerState> mRasterizerState;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<VisualEffect> mEffect;
    std::shared_ptr<MeshBuffer> mMeshBuffer;
    std::shared_ptr<ShadowVolumeNode> mShadow;

    BoundingBox<float> mBoundingBox;

    float mHeight;
    float mDensity;
    float mThickness;
    SColorF mColorDiffuse;
    SColorF mColorBright;
    SColorF mColorAmbient;
    Vector2<float> mSpeed;
    Vector2<float> mOrigin;
    unsigned int mSeed;

    bool mCameraInsideCloud;
    Vector3<short> mCameraOffset;
    Vector3<float> mCameraPosition;

    float mBlockSize;
    float mCloudSize;
    unsigned short mRadius;
    bool mEnable3D;
};

#endif