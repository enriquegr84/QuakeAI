// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef ANIMATEDMESH_H
#define ANIMATEDMESH_H

#include "GameEngineStd.h"

#include "Graphic/Resource/Buffer/MeshBuffer.h"
#include "Graphic/Scene/Mesh/Mesh.h"

//! Simple implementation of the BaseMesh interface.
class AnimatedMesh : public BaseAnimatedMesh
{
public:
	//! constructor
    AnimatedMesh(BaseMesh* mesh = 0) : mFramesPerSecond(25.f)
	{
        AddMesh(mesh);
        RecalculateBoundingBox();
	}

	//! destructor
	virtual ~AnimatedMesh()
	{
        mMeshes.clear();
	}


    //! Gets the frame count of the animated mesh.
    /** \return Amount of frames. If the amount is 1, it is a static, non animated mesh. */
    virtual size_t GetFrameCount() const
    {
        return mMeshes.size();
    }

    //! Gets the default animation speed of the animated mesh.
    /** \return Amount of frames per second. If the amount is 0, it is a static, non animated mesh. */
    virtual float GetAnimationSpeed() const
    {
        return mFramesPerSecond;
    }

    //! Gets the frame count of the animated mesh.
    /** \param fps Frames per second to play the animation with. If the amount is 0, it is not animated.
    The actual speed is set in the scene node the mesh is instantiated in.*/
    virtual void SetAnimationSpeed(float fps)
    {
        mFramesPerSecond = fps;
    }

	//! returns amount of mesh buffers.
	virtual size_t GetMeshBufferCount() const
	{
        size_t count = 0;
        for (auto const& mesh : mMeshes)
            count += mesh->GetMeshBufferCount();

		return count;
	}

	//! Get pointer to a mesh buffer.
	virtual std::shared_ptr<BaseMeshBuffer> GetMeshBuffer(unsigned int nr) const
	{
        for (auto const& mesh : mMeshes)
            return mesh->GetMeshBuffer(nr);

        return nullptr;
	}

	//! Get pointer to a mesh buffer which fits a material
	virtual std::shared_ptr<BaseMeshBuffer> GetMeshBuffer(const Material &material) const
	{
        for (auto const& mesh : mMeshes)
            return mesh->GetMeshBuffer(material);

		return nullptr;
	}

	//! Returns the type of the animated mesh.
	virtual MeshType GetMeshType() const
	{
		return MT_ANIMATED;
	}

	//! Adds a new meshbuffer to the mesh, access it as last one
	virtual void AddMesh(BaseMesh* mesh) 
	{
		mMeshes.push_back(std::shared_ptr<BaseMesh>(mesh));
	}

    //! Returns an axis aligned bounding box of the mesh.
    /** \return A bounding box of this mesh is returned. */
    virtual BoundingBox<float>& GetBoundingBox()
    {
        return mBoundingBox;
    }

    //! Recalculates the bounding box.
    void RecalculateBoundingBox()
    {
        mBoundingBox.Reset(0, 0, 0);

        if (mMeshes.empty())
            return;

        mBoundingBox = mMeshes[0]->GetBoundingBox();
        for (unsigned int i = 1; i < mMeshes.size(); ++i)
            mBoundingBox.GrowToContain(mMeshes[i]->GetBoundingBox());
    }

    //! Adds a new meshbuffer to the mesh, access it as last one
    virtual void AddMeshBuffer(BaseMeshBuffer* meshBuffer)
    {
        mMeshes[0]->AddMeshBuffer(meshBuffer);
    }

    //! Returns the IMesh interface for a frame.
    virtual std::shared_ptr<BaseMesh> GetMesh(int frame, 
        int detailLevel, int startFrameLoop, int endFrameLoop)
    {
        if (mMeshes.empty())
            return 0;

        return mMeshes[frame];
    }

protected:

    //! Default animation speed of this mesh.
    float mFramesPerSecond;

	//! The meshbuffer of this mesh
	std::vector<std::shared_ptr<BaseMesh>> mMeshes;

    //! The bounding box of this mesh
    BoundingBox<float> mBoundingBox;

};

#endif

