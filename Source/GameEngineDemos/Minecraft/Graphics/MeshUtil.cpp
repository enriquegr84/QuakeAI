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

#include "MeshUtil.h"

#include "Graphic/Scene/Mesh/NormalMesh.h"
#include "Graphic/Scene/Mesh/AnimatedMesh.h"

#include "Mathematic/Geometric/Hyperplane.h"

inline static void ApplyShadeFactor(SColor& color, float factor)
{
    color.SetRed((uint32_t)std::clamp(round(color.GetRed() * factor), 0.f, 255.f));
    color.SetGreen((uint32_t)std::clamp(round(color.GetGreen() * factor), 0.f, 255.f));
    color.SetBlue((uint32_t)std::clamp(round(color.GetBlue() * factor), 0.f, 255.f));
}

void ApplyFacesShading(SColor& color, const Vector3<float>& normal)
{
	/*
		Some drawtypes have normals set to (0, 0, 0), this must result in
		maximum brightness: shade factor 1.0.
		Shade factors for aligned cube faces are:
		+Y 1.000000 sqrt(1.0)
		-Y 0.447213 sqrt(0.2)
		+-X 0.670820 sqrt(0.45)
		+-Z 0.836660 sqrt(0.7)
	*/
	float x2 = normal[0] * normal[0];
	float y2 = normal[1] * normal[1];
	float z2 = normal[2] * normal[2];
	if (normal[1] < 0)
		ApplyShadeFactor(color, 0.670820f * x2 + 0.447213f * y2 + 0.836660f * z2);
	else if ((x2 > 1e-3) || (z2 > 1e-3))
		ApplyShadeFactor(color, 0.670820f * x2 + 1.000000f * y2 + 0.836660f * z2);
}

std::shared_ptr<BaseAnimatedMesh> CreateCubeMesh(Vector3<float> scale)
{
    SColorF c(1.f, 1.f, 1.f, 1.f);

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

    NormalMesh* mesh = new NormalMesh();
    { 
        //Up vertices
        MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
        meshBuffer->Position(0) = Vector3<float>{ -0.5, +0.5, -0.5 };
        meshBuffer->Position(1) = Vector3<float>{ -0.5, +0.5, +0.5 };
        meshBuffer->Position(2) = Vector3<float>{ +0.5, +0.5, +0.5 };
        meshBuffer->Position(3) = Vector3<float>{ +0.5, +0.5, -0.5 };

        meshBuffer->Normal(0) = Vector3<float>{ 0, 1, 0 };
        meshBuffer->Normal(1) = Vector3<float>{ 0, 1, 0 };
        meshBuffer->Normal(2) = Vector3<float>{ 0, 1, 0 };
        meshBuffer->Normal(3) = Vector3<float>{ 0, 1, 0 };

        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
        {
            meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 0.0f, 1.0f };
            meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 0.0f, 0.0f };
            meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 1.0f, 0.0f };
            meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 1.0f, 1.0f };

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

        // Set material
        meshBuffer->GetMaterial()->mLighting = false;
        meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
        meshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;

        meshBuffer->GetMaterial()->mBlendTarget.enable = true;
        meshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        meshBuffer->GetMaterial()->mDepthBuffer = true;
        meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

        meshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
        meshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;
        
        // Add mesh buffer to mesh
        mesh->AddMeshBuffer(meshBuffer);
    }

    {
        // Down
        MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
        meshBuffer->Position(0) = Vector3<float>{ -0.5, -0.5, -0.5 };
        meshBuffer->Position(1) = Vector3<float>{ +0.5, -0.5, -0.5 };
        meshBuffer->Position(2) = Vector3<float>{ +0.5, -0.5, +0.5 };
        meshBuffer->Position(3) = Vector3<float>{ -0.5, -0.5, +0.5 };

        meshBuffer->Normal(0) = Vector3<float>{ 0, -1, 0 };
        meshBuffer->Normal(1) = Vector3<float>{ 0, -1, 0 };
        meshBuffer->Normal(2) = Vector3<float>{ 0, -1, 0 };
        meshBuffer->Normal(3) = Vector3<float>{ 0, -1, 0 };

        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
        {
            meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 0.0f, 0.0f };
            meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 1.0f, 0.0f };
            meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 1.0f, 1.0f };
            meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 0.0f, 1.0f };

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

        // Set material
        meshBuffer->GetMaterial()->mLighting = false;
        meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
        meshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;

        meshBuffer->GetMaterial()->mBlendTarget.enable = true;
        meshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        meshBuffer->GetMaterial()->mDepthBuffer = true;
        meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

        meshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
        meshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;

        // Add mesh buffer to mesh
        mesh->AddMeshBuffer(meshBuffer);
    }

    {
        // Right
        MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
        meshBuffer->Position(0) = Vector3<float>{ +0.5, -0.5, -0.5 };
        meshBuffer->Position(1) = Vector3<float>{ +0.5, +0.5, -0.5 };
        meshBuffer->Position(2) = Vector3<float>{ +0.5, +0.5, +0.5 };
        meshBuffer->Position(3) = Vector3<float>{ +0.5, -0.5, +0.5 };

        meshBuffer->Normal(0) = Vector3<float>{ 1, 0, 0 };
        meshBuffer->Normal(1) = Vector3<float>{ 1, 0, 0 };
        meshBuffer->Normal(2) = Vector3<float>{ 1, 0, 0 };
        meshBuffer->Normal(3) = Vector3<float>{ 1, 0, 0 };

        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
        {
            meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 0.0f, 1.0f };
            meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 0.0f, 0.0f };
            meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 1.0f, 0.0f };
            meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 1.0f, 1.0f };

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
                (unsigned int)0 + vertices, (unsigned int)2 + vertices, (unsigned int)1 + vertices);
            meshBuffer->GetIndice()->SetTriangle(i + 1,
                (unsigned int)0 + vertices, (unsigned int)3 + vertices, (unsigned int)2 + vertices);
        }

        // Set material
        meshBuffer->GetMaterial()->mLighting = false;
        meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
        meshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;

        meshBuffer->GetMaterial()->mBlendTarget.enable = true;
        meshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        meshBuffer->GetMaterial()->mDepthBuffer = true;
        meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

        meshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
        meshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;

        // Add mesh buffer to mesh
        mesh->AddMeshBuffer(meshBuffer);
    }

    {
        // Left
        MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
        meshBuffer->Position(0) = Vector3<float>{ -0.5, -0.5, -0.5 };
        meshBuffer->Position(1) = Vector3<float>{ -0.5, -0.5, +0.5 };
        meshBuffer->Position(2) = Vector3<float>{ -0.5, +0.5, +0.5 };
        meshBuffer->Position(3) = Vector3<float>{ -0.5, +0.5, -0.5 };

        meshBuffer->Normal(0) = Vector3<float>{ -1, 0, 0 };
        meshBuffer->Normal(1) = Vector3<float>{ -1, 0, 0 };
        meshBuffer->Normal(2) = Vector3<float>{ -1, 0, 0 };
        meshBuffer->Normal(3) = Vector3<float>{ -1, 0, 0 };

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

        // Set material
        meshBuffer->GetMaterial()->mLighting = false;
        meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
        meshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;

        meshBuffer->GetMaterial()->mBlendTarget.enable = true;
        meshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        meshBuffer->GetMaterial()->mDepthBuffer = true;
        meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

        meshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
        meshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;

        // Add mesh buffer to mesh
        mesh->AddMeshBuffer(meshBuffer);
    }

    {
        // Back
        MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
        meshBuffer->Position(0) = Vector3<float>{ -0.5, -0.5, +0.5 };
        meshBuffer->Position(1) = Vector3<float>{ +0.5, -0.5, +0.5 };
        meshBuffer->Position(2) = Vector3<float>{ +0.5, +0.5, +0.5 };
        meshBuffer->Position(3) = Vector3<float>{ -0.5, +0.5, +0.5 };

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

        // Set material
        meshBuffer->GetMaterial()->mLighting = false;
        meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
        meshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;

        meshBuffer->GetMaterial()->mBlendTarget.enable = true;
        meshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        meshBuffer->GetMaterial()->mDepthBuffer = true;
        meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

        meshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
        meshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;

        // Add mesh buffer to mesh
        mesh->AddMeshBuffer(meshBuffer);
    }

    {
        // Front
        MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
        meshBuffer->Position(0) = Vector3<float>{ -0.5, -0.5, -0.5 };
        meshBuffer->Position(1) = Vector3<float>{ -0.5, +0.5, -0.5 };
        meshBuffer->Position(2) = Vector3<float>{ +0.5, +0.5, -0.5 };
        meshBuffer->Position(3) = Vector3<float>{ +0.5, -0.5, -0.5 };

        meshBuffer->Normal(0) = Vector3<float>{ 0, 0, -1 };
        meshBuffer->Normal(1) = Vector3<float>{ 0, 0, -1 };
        meshBuffer->Normal(2) = Vector3<float>{ 0, 0, -1 };
        meshBuffer->Normal(3) = Vector3<float>{ 0, 0, -1 };

        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
        {
            meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ 0.0f, 1.0f };
            meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ 0.0f, 0.0f };
            meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ 1.0f, 0.0f };
            meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ 1.0f, 1.0f };

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

        // Set material
        meshBuffer->GetMaterial()->mLighting = false;
        meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;
        meshBuffer->GetMaterial()->mType = MT_TRANSPARENT_ALPHA_CHANNEL_REF;

        meshBuffer->GetMaterial()->mBlendTarget.enable = true;
        meshBuffer->GetMaterial()->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
        meshBuffer->GetMaterial()->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        meshBuffer->GetMaterial()->mDepthBuffer = true;
        meshBuffer->GetMaterial()->mDepthMask = DepthStencilState::MASK_ALL;

        meshBuffer->GetMaterial()->mFillMode = RasterizerState::FILL_SOLID;
        meshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_NONE;

        // Add mesh buffer to mesh
        mesh->AddMeshBuffer(meshBuffer);
    }

    std::shared_ptr<BaseAnimatedMesh> animMesh(new AnimatedMesh(mesh));
	ScaleMesh(animMesh, scale);  // also recalculates bounding box
	return animMesh;
}

void ScaleMesh(std::shared_ptr<BaseMesh> mesh, Vector3<float> scale)
{
	if (mesh == NULL)
		return;

	unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
	for (unsigned int j = 0; j < mc; j++) 
    {
		std::shared_ptr<BaseMeshBuffer> meshBuffer = mesh->GetMeshBuffer(j);
        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
            meshBuffer->Position(i) *= scale;
	}
    RecalculateBoundingBox(mesh);
}

void TranslateMesh(std::shared_ptr<BaseMesh> mesh, Vector3<float> vec)
{
	if (mesh == NULL)
		return;

    unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
    for (unsigned int j = 0; j < mc; j++)
    {
        std::shared_ptr<BaseMeshBuffer> meshBuffer = mesh->GetMeshBuffer(j);
        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
            meshBuffer->Position(i) += vec;
    }
    RecalculateBoundingBox(mesh);
}

void SetMeshBufferColor(std::shared_ptr<BaseMeshBuffer> meshBuffer, const SColor& color)
{
    for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
        meshBuffer->Color(0, i) = SColorF(color).ToArray();
}

void SetAnimatedMeshColor(std::shared_ptr<AnimatedObjectMeshNode> node, const SColor& color)
{
	for (unsigned int i = 0; i < node->GetMaterialCount(); ++i)
		node->GetMaterial(i)->mEmissive = SColorF(color).ToArray();
}

void SetMeshColor(std::shared_ptr<BaseMesh> mesh, const SColor& color)
{
	if (mesh == NULL)
		return;

	unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
	for (unsigned int j = 0; j < mc; j++)
		SetMeshBufferColor(mesh->GetMeshBuffer(j), color);
}

void SetMeshBufferTextureCoords(std::shared_ptr<BaseMeshBuffer> meshBuffer, const Vector2<float>* uv, unsigned int count)
{
    LogAssert(meshBuffer->GetVertice()->GetNumElements() >= count, "invalid number");
    for (unsigned int i = 0; i < count; i++)
        meshBuffer->TCoord(0, i) = *uv;
}

template <typename F>
static void ApplyToMesh(std::shared_ptr<BaseMesh> mesh, const F& fn)
{
    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };

	for (unsigned int j = 0; j < mesh->GetMeshBufferCount(); j++)
    {
		std::shared_ptr<BaseMeshBuffer> meshBuffer = mesh->GetMeshBuffer(j);
        for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
            fn(meshBuffer->GetVertice()->Get<Vertex>(), i);
	}
}

void ColorizeMeshBuffer(std::shared_ptr<BaseMeshBuffer> meshBuffer, const SColor* bufferColor)
{
    SColor vc = *bufferColor;
    for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
    {
		// Apply shading
		ApplyFacesShading(vc, meshBuffer->Normal(i));
        meshBuffer->Color(0, i) = SColorF(vc).ToArray();
	}
}

void SetMeshColorByNormalXYZ(std::shared_ptr<BaseMesh> mesh,
    const SColor& colorX, const SColor& colorY, const SColor& colorZ)
{
	if (!mesh)
		return;

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
	auto colorizator = [=] (void* vtx, unsigned int i)
    {
        Vertex* vertex = (Vertex*)vtx;
		float x = fabs(vertex[i].normal[0]);
		float y = fabs(vertex[i].normal[1]);
		float z = fabs(vertex[i].normal[2]);
		if (x >= y && x >= z)
			vertex[i].color = SColorF(colorX).ToArray();
		else if (y >= z)
			vertex[i].color = SColorF(colorY).ToArray();
		else
			vertex[i].color = SColorF(colorZ).ToArray();
	};
	ApplyToMesh(mesh, colorizator);
}

void SetMeshColorByNormal(std::shared_ptr<BaseMesh> mesh,
    const Vector3<float>& normal, const SColor& color)
{
	if (!mesh)
		return;

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
	auto colorizator = [normal, color] (void* vtx, unsigned int i)
    {
        Vertex* vertex = (Vertex*)vtx;
		if (vertex[i].normal == normal)
			vertex[i].color = SColorF(color).ToArray();
	};
	ApplyToMesh(mesh, colorizator);
}

void RotateMeshXYBy(std::shared_ptr<BaseMesh> mesh, float degrees)
{
    degrees *= (float)GE_C_PI / 180.f;
    float c = std::cos(degrees);
    float s = std::sin(degrees);

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
    auto rotator = [c, s](void* vtx, unsigned int i)
    {
        Vertex* vertex = (Vertex*)vtx;
        float u = vertex[i].position[0];
        float v = vertex[i].position[1];
        vertex[i].position[0] = c * u - s * v;
        vertex[i].position[1] = s * u + c * v;
    };
    ApplyToMesh(mesh, rotator);
}

void RotateMeshXZBy(std::shared_ptr<BaseMesh> mesh, float degrees)
{
    degrees *= (float)GE_C_PI / 180.0f;
    float c = std::cos(degrees);
    float s = std::sin(degrees);

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
    auto rotator = [c, s](void* vtx, unsigned int i)
    {
        Vertex* vertex = (Vertex*)vtx;
        float u = vertex[i].position[0];
        float v = vertex[i].position[2];
        vertex[i].position[0] = c * u - s * v;
        vertex[i].position[2] = s * u + c * v;
    };
    ApplyToMesh(mesh, rotator);
}

void RotateMeshYZBy(std::shared_ptr<BaseMesh> mesh, float degrees)
{
    degrees *= (float)GE_C_PI / 180.0f;
    float c = std::cos(degrees);
    float s = std::sin(degrees);

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
        Vector3<float> normal;
    };
    auto rotator = [c, s](void* vtx, unsigned int i)
    {
        Vertex* vertex = (Vertex*)vtx;
        float u = vertex[i].position[1];
        float v = vertex[i].position[2];
        vertex[i].position[1] = c * u - s * v;
        vertex[i].position[2] = s * u + c * v;
    };
    ApplyToMesh(mesh, rotator);
}

void RotateMeshBy6DFaceDir(std::shared_ptr<BaseMesh> mesh, int facedir)
{
	int axisdir = facedir >> 2;
	facedir &= 0x03;
	switch (facedir) 
    {
		case 1: RotateMeshXZBy(mesh, -90); break;
		case 2: RotateMeshXZBy(mesh, 180); break;
		case 3: RotateMeshXZBy(mesh, 90); break;
	}
	switch (axisdir) 
    {
		case 1: RotateMeshYZBy(mesh, 90); break; // z+
		case 2: RotateMeshYZBy(mesh, -90); break; // z-
		case 3: RotateMeshXYBy(mesh, -90); break; // x+
		case 4: RotateMeshXYBy(mesh, 90); break; // x-
		case 5: RotateMeshXYBy(mesh, -180); break;
	}
}

bool CheckMeshNormals(std::shared_ptr<BaseMesh> mesh)
{
	unsigned int bufferCount = (unsigned int)mesh->GetMeshBufferCount();

	for (unsigned int i = 0; i < bufferCount; i++) 
    {
		std::shared_ptr<BaseMeshBuffer> buffer = mesh->GetMeshBuffer(i);

		// Here we intentionally check only first normal, assuming that if buffer
		// has it valid, then most likely all other ones are fine too. We can
		// check all of the normals to have length, but it seems like an overkill
		// hurting the performance and covering only really weird broken models.
		float length = Length(buffer->Normal(0));
		if (!std::isfinite(length) || length < 1e-10f)
			return false;
	}

	return true;
}

BaseMeshBuffer* CloneMeshBuffer(std::shared_ptr<BaseMeshBuffer> meshBuffer)
{
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

    MeshBuffer* clonedMeshBuffer = new MeshBuffer(vformat,
        meshBuffer->GetVertice()->GetNumElements(),
        meshBuffer->GetIndice()->GetNumPrimitives(),
        meshBuffer->GetIndice()->GetElementSize());

    // fill vertices
    for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i++)
    {
        clonedMeshBuffer->Position(i) = meshBuffer->Position(i);
        clonedMeshBuffer->Normal(i) = meshBuffer->Normal(i);
        clonedMeshBuffer->Color(0, i) = meshBuffer->Color(0, i);
        clonedMeshBuffer->TCoord(0, i) = meshBuffer->TCoord(0, i);
    }

    // fill indices
    int idx = 0;
    unsigned int* indices = meshBuffer->GetIndice()->Get<unsigned int>();
    for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i += 2, idx += 6)
    {
        clonedMeshBuffer->GetIndice()->SetTriangle(i,
            indices[idx+0], indices[idx+1], indices[idx+2]);
        clonedMeshBuffer->GetIndice()->SetTriangle(i + 1,
            indices[idx+3], indices[idx+4], indices[idx+5]);
    }

    return clonedMeshBuffer;
}

std::shared_ptr<BaseMesh> CloneMesh(std::shared_ptr<BaseMesh> srcMesh)
{
    std::shared_ptr<NormalMesh> dstMesh(new NormalMesh());
	for (unsigned int j = 0; j < srcMesh->GetMeshBufferCount(); j++) 
    {
        BaseMeshBuffer* meshBuffer = CloneMeshBuffer(srcMesh->GetMeshBuffer(j));
        dstMesh->AddMeshBuffer(meshBuffer);
	}
    RecalculateBoundingBox(dstMesh);

	return dstMesh;
}

std::shared_ptr<BaseMesh> ConvertNodeBoxesToMesh(
    const std::vector<BoundingBox<float>>& boxes, const float* uvCoords, float expand)
{
    std::shared_ptr<NormalMesh> dstMesh(new NormalMesh());
	SColorF c(1.f, 1.f, 1.f, 1.f);

	for (BoundingBox<float> box : boxes) 
    {
		box.Repair();

		box.mMinEdge[0] -= expand;
		box.mMinEdge[1] -= expand;
		box.mMinEdge[2] -= expand;
		box.mMaxEdge[0] += expand;
		box.mMaxEdge[1] += expand;
		box.mMaxEdge[2] += expand;

		// Compute texture UV coords
		float tx1 = (box.mMinEdge[0] / BS) + 0.5f;
		float ty1 = (box.mMinEdge[1] / BS) + 0.5f;
		float tz1 = (box.mMinEdge[2] / BS) + 0.5f;
		float tx2 = (box.mMaxEdge[0] / BS) + 0.5f;
		float ty2 = (box.mMaxEdge[1] / BS) + 0.5f;
		float tz2 = (box.mMaxEdge[2] / BS) + 0.5f;

		float txcDefault[24] = {
			// up
			tx1, 1 - tz2, tx2, 1 - tz1,
			// down
			tx1, tz1, tx2, tz2,
			// right
			tz1, 1 - ty2, tz2, 1 - ty1,
			// left
			1 - tz2, 1 - ty2, 1 - tz1, 1 - ty1,
			// back
			1 - tx2, 1 - ty2, 1 - tx1, 1 - ty1,
			// front
			tx1, 1 - ty2, tx2, 1 - ty1,
		};

		// use default texture UV mapping if not provided
		const float* txc = uvCoords ? uvCoords : txcDefault;

		Vector3<float> min = box.mMinEdge;
		Vector3<float> max = box.mMaxEdge;

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

        {
            // up
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
            meshBuffer->Position(0) = Vector3<float>{ min[0], max[1], max[2] };
            meshBuffer->Position(1) = Vector3<float>{ max[0], max[1], max[2] };
            meshBuffer->Position(2) = Vector3<float>{ max[0], max[1], min[2] };
            meshBuffer->Position(3) = Vector3<float>{ min[0], max[1], min[2] };

            meshBuffer->Normal(0) = Vector3<float>{ 0, 1, 0 };
            meshBuffer->Normal(1) = Vector3<float>{ 0, 1, 0 };
            meshBuffer->Normal(2) = Vector3<float>{ 0, 1, 0 };
            meshBuffer->Normal(3) = Vector3<float>{ 0, 1, 0 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ txc[0], txc[1] };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ txc[2], txc[1] };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ txc[2], txc[3] };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ txc[0], txc[3] };

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

            // Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;

            // Add mesh buffer to mesh
            dstMesh->AddMeshBuffer(meshBuffer);
        }

        {
            // down
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
            meshBuffer->Position(0) = Vector3<float>{ min[0], min[1], min[2] };
            meshBuffer->Position(1) = Vector3<float>{ max[0], min[1], min[2] };
            meshBuffer->Position(2) = Vector3<float>{ max[0], min[1], max[2] };
            meshBuffer->Position(3) = Vector3<float>{ min[0], min[1], max[2] };

            meshBuffer->Normal(0) = Vector3<float>{ 0, -1, 0 };
            meshBuffer->Normal(1) = Vector3<float>{ 0, -1, 0 };
            meshBuffer->Normal(2) = Vector3<float>{ 0, -1, 0 };
            meshBuffer->Normal(3) = Vector3<float>{ 0, -1, 0 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ txc[4], txc[5] };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ txc[6], txc[5] };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ txc[6], txc[7] };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ txc[4], txc[7] };

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

            // Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;

            // Add mesh buffer to mesh
            dstMesh->AddMeshBuffer(meshBuffer);
        }

        {
            // right
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
            meshBuffer->Position(0) = Vector3<float>{ max[0], max[1], min[2] };
            meshBuffer->Position(1) = Vector3<float>{ max[0], max[1], max[2] };
            meshBuffer->Position(2) = Vector3<float>{ max[0], min[1], max[2] };
            meshBuffer->Position(3) = Vector3<float>{ max[0], min[1], min[2] };

            meshBuffer->Normal(0) = Vector3<float>{ 1, 0, 0 };
            meshBuffer->Normal(1) = Vector3<float>{ 1, 0, 0 };
            meshBuffer->Normal(2) = Vector3<float>{ 1, 0, 0 };
            meshBuffer->Normal(3) = Vector3<float>{ 1, 0, 0 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ txc[8], txc[9] };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ txc[10], txc[9] };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ txc[10], txc[11] };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ txc[8], txc[11] };

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

            // Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;

            // Add mesh buffer to mesh
            dstMesh->AddMeshBuffer(meshBuffer);
        }

        {
            // left
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
            meshBuffer->Position(0) = Vector3<float>{ min[0], max[1], max[2] };
            meshBuffer->Position(1) = Vector3<float>{ min[0], max[1], min[2] };
            meshBuffer->Position(2) = Vector3<float>{ min[0], min[1], min[2] };
            meshBuffer->Position(3) = Vector3<float>{ min[0], min[1], max[2] };

            meshBuffer->Normal(0) = Vector3<float>{ -1, 0, 0 };
            meshBuffer->Normal(1) = Vector3<float>{ -1, 0, 0 };
            meshBuffer->Normal(2) = Vector3<float>{ -1, 0, 0 };
            meshBuffer->Normal(3) = Vector3<float>{ -1, 0, 0 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ txc[12], txc[13] };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ txc[14], txc[13] };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ txc[14], txc[15] };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ txc[12], txc[15] };

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

            // Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;

            // Add mesh buffer to mesh
            dstMesh->AddMeshBuffer(meshBuffer);
        }

        {
            // back
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
            meshBuffer->Position(0) = Vector3<float>{ max[0], max[1], max[2] };
            meshBuffer->Position(1) = Vector3<float>{ min[0], max[1], max[2] };
            meshBuffer->Position(2) = Vector3<float>{ min[0], min[1], max[2] };
            meshBuffer->Position(3) = Vector3<float>{ max[0], min[1], max[2] };

            meshBuffer->Normal(0) = Vector3<float>{ 0, 0, 1 };
            meshBuffer->Normal(1) = Vector3<float>{ 0, 0, 1 };
            meshBuffer->Normal(2) = Vector3<float>{ 0, 0, 1 };
            meshBuffer->Normal(3) = Vector3<float>{ 0, 0, 1 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ txc[16], txc[17] };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ txc[18], txc[17] };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ txc[18], txc[19] };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ txc[16], txc[19] };

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

            // Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;

            // Add mesh buffer to mesh
            dstMesh->AddMeshBuffer(meshBuffer);
        }

        {
            // front
            MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
            meshBuffer->Position(0) = Vector3<float>{ min[0], max[1], min[2] };
            meshBuffer->Position(1) = Vector3<float>{ max[0], max[1], min[2] };
            meshBuffer->Position(2) = Vector3<float>{ max[0], min[1], min[2] };
            meshBuffer->Position(3) = Vector3<float>{ min[0], min[1], min[2] };

            meshBuffer->Normal(0) = Vector3<float>{ 0, 0, -1 };
            meshBuffer->Normal(1) = Vector3<float>{ 0, 0, -1 };
            meshBuffer->Normal(2) = Vector3<float>{ 0, 0, -1 };
            meshBuffer->Normal(3) = Vector3<float>{ 0, 0, -1 };

            for (unsigned int i = 0; i < meshBuffer->GetVertice()->GetNumElements(); i += 4)
            {
                meshBuffer->TCoord(0, 0 + i) = Vector2<float>{ txc[20], txc[21] };
                meshBuffer->TCoord(0, 1 + i) = Vector2<float>{ txc[22], txc[21] };
                meshBuffer->TCoord(0, 2 + i) = Vector2<float>{ txc[22], txc[23] };
                meshBuffer->TCoord(0, 3 + i) = Vector2<float>{ txc[20], txc[23] };

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

            // Set material
            meshBuffer->GetMaterial()->mLighting = false;
            meshBuffer->GetMaterial()->mTextureLayer[0].mFilter = SamplerState::ANISOTROPIC;

            // Add mesh buffer to mesh
            dstMesh->AddMeshBuffer(meshBuffer);
        }
	}
    RecalculateBoundingBox(dstMesh);

	return dstMesh;
}

struct VCache
{
	std::vector<unsigned int> tris;
	float score;
    int16_t cachePos;
    uint16_t numActiveTris;
};

struct TCache
{
	uint16_t ind[3];
	float score;
	bool drawn;
};

const uint16_t cacheSize = 32;

float FindVertexScore(VCache* v)
{
	const float cacheDecayPower = 1.5f;
	const float lastTriScore = 0.75f;
	const float valenceBoostScale = 2.0f;
	const float valenceBoostPower = 0.5f;
	const float maxSizeVertexCache = 32.0f;

	if (v->numActiveTris == 0)
	{
		// No tri needs this vertex!
		return -1.0f;
	}

	float score = 0.0f;
	int cachePosition = v->cachePos;
	if (cachePosition < 0)
	{
		// Vertex is not in FIFO cache - no score.
	}
	else
	{
		if (cachePosition < 3)
		{
			// This vertex was used in the last triangle,
			// so it has a fixed score.
			score = lastTriScore;
		}
		else
		{
			// Points for being high in the cache.
			const float Scaler = 1.0f / (maxSizeVertexCache - 3);
			score = 1.0f - (cachePosition - 3) * Scaler;
			score = powf(score, cacheDecayPower);
		}
	}

	// Bonus points for having a low number of tris still to
	// use the vert, so we get rid of lone verts quickly.
	float valenceBoost = powf(v->numActiveTris, -valenceBoostPower);
	score += valenceBoostScale * valenceBoost;

	return score;
}

/*
	A specialized LRU cache for the Forsyth algorithm.
*/

class FLRU
{

public:
	FLRU(VCache* v, TCache* t): vc(v), tc(t)
	{
		for (int &i : cache)
			i = -1;
	}

	// Adds this vertex index and returns the highest-scoring triangle index
	unsigned int Add(uint16_t vert, bool updatetris = false)
	{
		bool found = false;

		// Mark existing pos as empty
		for (uint16_t i = 0; i < cacheSize; i++)
		{
			if (cache[i] == vert)
			{
				// Move everything down
				for (uint16_t j = i; j; j--)
					cache[j] = cache[j - 1];

				found = true;
				break;
			}
		}

		if (!found)
		{
			if (cache[cacheSize-1] != -1)
				vc[cache[cacheSize-1]].cachePos = -1;

			// Move everything down
			for (uint16_t i = cacheSize - 1; i; i--)
				cache[i] = cache[i - 1];
		}

		cache[0] = vert;

		unsigned int highest = 0;
		float hiscore = 0;

		if (updatetris)
		{
			// Update cache positions
			for (uint16_t i = 0; i < cacheSize; i++)
			{
				if (cache[i] == -1)
					break;

				vc[cache[i]].cachePos = i;
				vc[cache[i]].score = FindVertexScore(&vc[cache[i]]);
			}

			// Update triangle scores
			for (int i : cache) {
				if (i == -1)
					break;

				for (size_t t = 0; t < vc[i].tris.size(); t++)
				{
					TCache* tri = &tc[vc[i].tris[t]];

					tri->score =
						vc[tri->ind[0]].score +
						vc[tri->ind[1]].score +
						vc[tri->ind[2]].score;

					if (tri->score > hiscore)
					{
						hiscore = tri->score;
						highest = vc[i].tris[t];
					}
				}
			}
		}

		return highest;
	}

private:
	int cache[cacheSize];
	VCache* vc;
	TCache* tc;
};


static Vector3<float> GetAngleWeight(const Vector3<float>& v1, 
    const Vector3<float>& v2, const Vector3<float>& v3)
{
    // Calculate this triangle's weight for each of its three vertices
    // start by calculating the lengths of its sides
    const float a = Length(v2 - v3);
    const float asqrt = sqrtf(a);
    const float b = Length(v1 - v3);
    const float bsqrt = sqrtf(b);
    const float c = Length(v1 - v2);
    const float csqrt = sqrtf(c);

    // use them to find the angle at each vertex
    return Vector3<float>{
        acosf((b + c - a) / (2.f * bsqrt * csqrt)),
        acosf((-b + c + a) / (2.f * asqrt * csqrt)),
        acosf((b - c + a) / (2.f * bsqrt * asqrt))};
}

void RecalculateBoundingBox(std::shared_ptr<BaseMesh> srcMesh)
{
    BoundingBox<float> bbox;
    bbox.Reset(0, 0, 0);
    for (unsigned short j = 0; j < srcMesh->GetMeshBufferCount(); j++) 
    {
        std::shared_ptr<BaseMeshBuffer> buf = srcMesh->GetMeshBuffer(j);
        buf->RecalculateBoundingBox();
        if (j == 0)
            bbox = buf->GetBoundingBox();
        else
            bbox.GrowToContain(buf->GetBoundingBox());
    }
    srcMesh->GetBoundingBox() = bbox;
}

void RecalculateNormalsT(std::shared_ptr<BaseMeshBuffer> buffer, bool smooth, bool angleWeighted)
{
    const unsigned int vtxcnt = buffer->GetVertice()->GetNumElements();
    const unsigned int  idxcnt = buffer->GetIndice()->GetNumElements();
    const unsigned int* idx = buffer->GetIndice()->Get<unsigned int>();

    if (!smooth)
    {
        for (unsigned int  i = 0; i < idxcnt; i += 3)
        {
            const Vector3<float>& v1 = buffer->Position(idx[i + 0]);
            const Vector3<float>& v2 = buffer->Position(idx[i + 1]);
            const Vector3<float>& v3 = buffer->Position(idx[i + 2]);
            const Vector3<float> normal = Plane3<float>({ v1, v2, v3 }).mNormal;
            buffer->Normal(idx[i + 0]) = normal;
            buffer->Normal(idx[i + 1]) = normal;
            buffer->Normal(idx[i + 2]) = normal;
        }
    }
    else
    {
        unsigned int  i;
        for (i = 0; i != vtxcnt; ++i)
            buffer->Normal(i) = Vector3<float>::Zero();

        for (i = 0; i < idxcnt; i += 3)
        {
            const Vector3<float>& v1 = buffer->Position(idx[i + 0]);
            const Vector3<float>& v2 = buffer->Position(idx[i + 1]);
            const Vector3<float>& v3 = buffer->Position(idx[i + 2]);
            const Vector3<float> normal = Plane3<float>({ v1, v2, v3 }).mNormal;

            Vector3<float> weight{ 1.f, 1.f, 1.f };
            if (angleWeighted)
                weight = GetAngleWeight(v1, v2, v3); // writing irr::scene:: necessary for borland

            buffer->Normal(idx[i + 0]) += weight[0]*normal;
            buffer->Normal(idx[i + 1]) += weight[1]*normal;
            buffer->Normal(idx[i + 2]) += weight[2]*normal;
        }

        for (i = 0; i != vtxcnt; ++i)
            Normalize(buffer->Normal(i));
    }
}


//! Recalculates all normals of the mesh buffer.
/** \param buffer: Mesh buffer on which the operation is performed. */
void RecalculateNormals(std::shared_ptr<BaseMeshBuffer> buffer, bool smooth, bool angleWeighted)
{
    if (!buffer)
        return;

    RecalculateNormalsT(buffer, smooth, angleWeighted);
}


//! Recalculates all normals of the mesh.
//! \param mesh: Mesh on which the operation is performed.
void RecalculateNormals(std::shared_ptr<BaseMesh> mesh, bool smooth, bool angleWeighted)
{
    if (!mesh)
        return;

    const unsigned int bcount = (unsigned int)mesh->GetMeshBufferCount();
    for (unsigned int b = 0; b < bcount; ++b)
        RecalculateNormals(mesh->GetMeshBuffer(b), smooth, angleWeighted);
}


/**
Vertex cache optimization according to the Forsyth paper:
http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html

The function is thread-safe (read: you can optimize several meshes in different threads)

\param mesh Source mesh for the operation.  */
std::shared_ptr<BaseMesh> CreateForsythOptimizedMesh(const std::shared_ptr<BaseMesh> mesh)
{
	if (!mesh)
		return 0;

    std::shared_ptr<NormalMesh> newMesh(new NormalMesh());

	const unsigned int mbCount = (unsigned int)mesh->GetMeshBufferCount();
	for (unsigned int b = 0; b < mbCount; ++b)
	{
		const std::shared_ptr<BaseMeshBuffer> mb = mesh->GetMeshBuffer(b);
        /*
		if (mb->GetIndice()->GetElementSize() != video::EIT_16BIT)
		{
			//os::Printer::log("Cannot optimize a mesh with 32bit indices", ELL_ERROR);
			newmesh->drop();
			return 0;
		}
        */
		const unsigned int icount = mb->GetIndice()->GetNumElements();
		const unsigned int tcount = icount / 3;
		const unsigned int vcount = mb->GetVertice()->GetNumElements();
		const unsigned int *ind = mb->GetIndice()->Get<unsigned int>();

		VCache* vc = new VCache[vcount];
		TCache* tc = new TCache[tcount];

		FLRU lru(vc, tc);

		// init
		for (unsigned int i = 0; i < vcount; i++)
		{
			vc[i].score = 0;
			vc[i].cachePos = -1;
			vc[i].numActiveTris = 0;
		}

		// First pass: count how many times a vert is used
		for (unsigned int i = 0; i < icount; i += 3)
		{
			vc[ind[i]].numActiveTris++;
			vc[ind[i + 1]].numActiveTris++;
			vc[ind[i + 2]].numActiveTris++;

			const unsigned int triInd = i/3;
			tc[triInd].ind[0] = ind[i];
			tc[triInd].ind[1] = ind[i + 1];
			tc[triInd].ind[2] = ind[i + 2];
		}

		// Second pass: list of each triangle
		for (unsigned int i = 0; i < tcount; i++)
		{
			vc[tc[i].ind[0]].tris.push_back(i);
			vc[tc[i].ind[1]].tris.push_back(i);
			vc[tc[i].ind[2]].tris.push_back(i);

			tc[i].drawn = false;
		}

		// Give initial scores
		for (unsigned int i = 0; i < vcount; i++)
			vc[i].score = FindVertexScore(&vc[i]);

		for (unsigned int i = 0; i < tcount; i++)
		{
			tc[i].score = 
                vc[tc[i].ind[0]].score + 
                vc[tc[i].ind[1]].score + 
                vc[tc[i].ind[2]].score;
		}

		{
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

            MeshBuffer* meshBuffer = new MeshBuffer(vformat, vcount, icount, sizeof(unsigned int));
            unsigned int* indices = meshBuffer->GetIndice()->Get<unsigned int>();
            meshBuffer->GetMaterial() = mb->GetMaterial();

            std::map<Vertex*, unsigned short> sInd; // search index for fast operation

			// Main algorithm
			unsigned int highest = 0;
			unsigned int drawcalls = 0;
            Vertex* vertex = mb->GetVertice()->Get<Vertex>();
			for (unsigned int vIndex = 0, iIndex = 0; ; )
			{
				if (tc[highest].drawn)
				{
					bool found = false;
					float hiscore = 0;
					for (unsigned int t = 0; t < tcount; t++)
					{
						if (!tc[t].drawn)
						{
							if (tc[t].score > hiscore)
							{
								highest = t;
								hiscore = tc[t].score;
								found = true;
							}
						}
					}
					if (!found)
						break;
				}

                // Output the best triangle
                std::map<Vertex*, unsigned short>::iterator sIt = sInd.find(&vertex[tc[highest].ind[0]]);
                if (sIt == sInd.end())
                {
                    // fill vertex
                    meshBuffer->Position(vIndex) = vertex[tc[highest].ind[0]].position;
                    meshBuffer->Normal(vIndex) = vertex[tc[highest].ind[0]].normal;
                    meshBuffer->Color(0, vIndex) = vertex[tc[highest].ind[0]].color;
                    meshBuffer->TCoord(0, vIndex) = vertex[tc[highest].ind[0]].tcoord;

                    // fill indices
                    indices[iIndex] = vIndex;
                    sInd[&vertex[tc[highest].ind[0]]] = vIndex;
                    vIndex++;
                    iIndex++;
				}
				else
				{
                    indices[iIndex] = (*sIt).second;
                    iIndex++;
				}

				sIt = sInd.find(&vertex[tc[highest].ind[1]]);
                if (sIt == sInd.end())
				{
                    // fill vertex
                    meshBuffer->Position(vIndex) = vertex[tc[highest].ind[1]].position;
                    meshBuffer->Normal(vIndex) = vertex[tc[highest].ind[1]].normal;
                    meshBuffer->Color(0, vIndex) = vertex[tc[highest].ind[1]].color;
                    meshBuffer->TCoord(0, vIndex) = vertex[tc[highest].ind[1]].tcoord;

                    // fill indices
                    indices[iIndex] = vIndex;
                    sInd[&vertex[tc[highest].ind[1]]] = vIndex;
                    vIndex++;
                    iIndex++;
				}
				else
				{
                    indices[iIndex] = (*sIt).second;
                    iIndex++;
				}

                sIt = sInd.find(&vertex[tc[highest].ind[2]]);
                if (sIt == sInd.end())
                {
                    // fill vertex
                    meshBuffer->Position(vIndex) = vertex[tc[highest].ind[2]].position;
                    meshBuffer->Normal(vIndex) = vertex[tc[highest].ind[2]].normal;
                    meshBuffer->Color(0, vIndex) = vertex[tc[highest].ind[2]].color;
                    meshBuffer->TCoord(0, vIndex) = vertex[tc[highest].ind[2]].tcoord;

                    // fill indices
                    indices[iIndex] = vIndex;
                    sInd[&vertex[tc[highest].ind[2]]] = vIndex;
                    vIndex++;
                    iIndex++;
                }
                else
                {
                    indices[iIndex] = (*sIt).second;
                    iIndex++;
                }

				vc[tc[highest].ind[0]].numActiveTris--;
				vc[tc[highest].ind[1]].numActiveTris--;
				vc[tc[highest].ind[2]].numActiveTris--;

				tc[highest].drawn = true;
				for (uint16_t j : tc[highest].ind) 
                {
					VCache* vert = &vc[j];
                    
                    auto it = std::find(vert->tris.begin(), vert->tris.end(), highest);
                    vert->tris.erase(it);
				}

				lru.Add(tc[highest].ind[0]);
				lru.Add(tc[highest].ind[1]);
				highest = lru.Add(tc[highest].ind[2], true);
				drawcalls++;
			}

			newMesh->AddMeshBuffer(meshBuffer);
		}

		delete [] vc;
		delete [] tc;

	} // for each meshbuffer

	return newMesh;
}
