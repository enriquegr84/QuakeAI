// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef NORMALMESH_H
#define NORMALMESH_H

#include "GameEngineStd.h"

#include "Graphic/Resource/Buffer/MeshBuffer.h"
#include "Graphic/Scene/Mesh/Mesh.h"

//! Simple implementation of the BaseMesh interface.
class NormalMesh : public BaseMesh
{
public:
	//! constructor
	NormalMesh()
	{
	}

	//! destructor
	virtual ~NormalMesh()
	{
	}

	//! clean mesh
	virtual void Clear()
	{
		mMeshBuffers.clear();
	}


	//! returns amount of mesh buffers.
	virtual size_t GetMeshBufferCount() const
	{
		return mMeshBuffers.size();
	}

	//! Get pointer to a mesh buffer.
	virtual std::shared_ptr<BaseMeshBuffer> GetMeshBuffer(unsigned int nr) const
	{
		if (mMeshBuffers.empty())
			return nullptr;

		if (nr < 0 || nr >= mMeshBuffers.size())
			return nullptr;

		return mMeshBuffers[nr];
	}

	//! Get pointer to a mesh buffer which fits a material
	virtual std::shared_ptr<BaseMeshBuffer> GetMeshBuffer(const Material &material) const
	{
		for (auto const& meshBuffer : mMeshBuffers)
		{
			if (&material == meshBuffer->GetMaterial().get())
				return meshBuffer;
		}

		return nullptr;
	}

	//! Returns the type of the animated mesh.
	virtual MeshType GetMeshType() const
	{
		return MT_NORMAL;
	}

	//! Adds a new meshbuffer to the mesh, access it as last one
	void AddMeshBuffer(BaseMeshBuffer* meshBuffer) 
	{
		mMeshBuffers.push_back(std::shared_ptr<MeshBuffer>((MeshBuffer*)meshBuffer));
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

        if (mMeshBuffers.empty())
            return;

        mBoundingBox = mMeshBuffers[0]->GetBoundingBox();
        for (unsigned int i = 1; i < mMeshBuffers.size(); ++i)
            mBoundingBox.GrowToContain(mMeshBuffers[i]->GetBoundingBox());
    }

protected:

	//! The meshbuffer of this mesh
	std::vector<std::shared_ptr<MeshBuffer>> mMeshBuffers;

    //! The bounding box of this mesh
    BoundingBox<float> mBoundingBox;
};

#endif

