/*
Minetest
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "WieldMesh.h"

#include "MeshCollector.h"
#include "MeshUtil.h"
#include "Shader.h"
#include "Sky.h"

#include "../Games/Games.h"
#include "../Games/Actors/Item.h"
#include "../Games/Actors/Inventory.h"

#include "../Games/Map/MapNode.h"

#include "../Graphics/Map/ContentMapBlock.h"
#include "../Graphics/Map/MapBlockMesh.h"

#include "../Utils/Util.h"

#include "Application/Settings.h"

#include "Graphic/Scene/Mesh/NormalMesh.h"

#define WIELD_SCALE_FACTOR 30.0
#define WIELD_SCALE_FACTOR_EXTRUDED 40.0

static std::shared_ptr<BaseMesh> CreateExtrusionMesh(int resolutionX, int resolutionY)
{
	const float r = 0.5;
    unsigned int index = 0;
	SColorF c(1.f, 1.f, 1.f, 1.f);
    Vector3<float> scale{ 1.0f, 1.0f, 0.1f };

    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
	vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);

    MeshBuffer* meshBuffer = new MeshBuffer(vformat, 
        8 * (resolutionX + resolutionY + 1), 
        4 * (resolutionX + resolutionY + 1),
        sizeof(unsigned int));

    {
        // z-
        meshBuffer->Position(0 + index) = Vector3<float>{ -r, +r, -r };
        meshBuffer->Position(1 + index) = Vector3<float>{ +r, +r, -r };
        meshBuffer->Position(2 + index) = Vector3<float>{ +r, -r, -r };
        meshBuffer->Position(3 + index) = Vector3<float>{ -r, -r, -r };

        meshBuffer->Normal(0 + index) = Vector3<float>{ 0, 0, -1 };
        meshBuffer->Normal(1 + index) = Vector3<float>{ 0, 0, -1 };
        meshBuffer->Normal(2 + index) = Vector3<float>{ 0, 0, -1 };
        meshBuffer->Normal(3 + index) = Vector3<float>{ 0, 0, -1 };

        meshBuffer->TCoord(0, 0 + index) = Vector2<float>{ 0.0f, 0.0f };
        meshBuffer->TCoord(0, 1 + index) = Vector2<float>{ 1.0f, 0.0f };
        meshBuffer->TCoord(0, 2 + index) = Vector2<float>{ 1.0f, 1.0f };
        meshBuffer->TCoord(0, 3 + index) = Vector2<float>{ 0.0f, 1.0f };

        meshBuffer->Color(0, 0 + index) = c.ToArray();
        meshBuffer->Color(0, 1 + index) = c.ToArray();
        meshBuffer->Color(0, 2 + index) = c.ToArray();
        meshBuffer->Color(0, 3 + index) = c.ToArray();
        index += 4;
    }
    
    {
		// z+
        meshBuffer->Position(0 + index) = Vector3<float>{ -r, +r ,+r };
        meshBuffer->Position(1 + index) = Vector3<float>{ -r, -r, +r };
        meshBuffer->Position(2 + index) = Vector3<float>{ +r, -r, +r };
        meshBuffer->Position(3 + index) = Vector3<float>{ +r, +r, +r };

        meshBuffer->Normal(0 + index) = Vector3<float>{ 0, 0, +1 };
        meshBuffer->Normal(1 + index) = Vector3<float>{ 0, 0, +1 };
        meshBuffer->Normal(2 + index) = Vector3<float>{ 0, 0, +1 };
        meshBuffer->Normal(3 + index) = Vector3<float>{ 0, 0, +1 };

        meshBuffer->TCoord(0, 0 + index) = Vector2<float>{ 0.0f, 0.0f };
        meshBuffer->TCoord(0, 1 + index) = Vector2<float>{ 0.0f, 1.0f };
        meshBuffer->TCoord(0, 2 + index) = Vector2<float>{ 1.0f, 1.0f };
        meshBuffer->TCoord(0, 3 + index) = Vector2<float>{ 1.0f, 0.0f };

        meshBuffer->Color(0, 0 + index) = c.ToArray();
        meshBuffer->Color(0, 1 + index) = c.ToArray();
        meshBuffer->Color(0, 2 + index) = c.ToArray();
        meshBuffer->Color(0, 3 + index) = c.ToArray();
        index += 4;
	}

	float pixelSizeX = 1 / (float) resolutionX;
	float pixelSizeY = 1 / (float) resolutionY;

	for (int i = 0; i < resolutionX; ++i) 
    {
		float pixelPosX = i * pixelSizeX - 0.5f;
		float x0 = pixelPosX;
		float x1 = pixelPosX + pixelSizeX;
		float tex0 = (i + 0.1f) * pixelSizeX;
		float tex1 = (i + 0.9f) * pixelSizeX;

        {
            // x-
            meshBuffer->Position(0 + index) = Vector3<float>{ x0, -r, -r };
            meshBuffer->Position(1 + index) = Vector3<float>{ x0, -r, +r };
            meshBuffer->Position(2 + index) = Vector3<float>{ x0, +r, +r };
            meshBuffer->Position(3 + index) = Vector3<float>{ x0, +r, -r };

            meshBuffer->Normal(0 + index) = Vector3<float>{ -1, 0, 0 };
            meshBuffer->Normal(1 + index) = Vector3<float>{ -1, 0, 0 };
            meshBuffer->Normal(2 + index) = Vector3<float>{ -1, 0, 0 };
            meshBuffer->Normal(3 + index) = Vector3<float>{ -1, 0, 0 };

            meshBuffer->TCoord(0, 0 + index) = Vector2<float>{ tex0, 1 };
            meshBuffer->TCoord(0, 1 + index) = Vector2<float>{ tex1, 1 };
            meshBuffer->TCoord(0, 2 + index) = Vector2<float>{ tex1, 0 };
            meshBuffer->TCoord(0, 3 + index) = Vector2<float>{ tex0, 0 };

            meshBuffer->Color(0, 0 + index) = c.ToArray();
            meshBuffer->Color(0, 1 + index) = c.ToArray();
            meshBuffer->Color(0, 2 + index) = c.ToArray();
            meshBuffer->Color(0, 3 + index) = c.ToArray();
            index += 4;
        }

        {
			// x+
            meshBuffer->Position(0 + index) = Vector3<float>{ x1, -r, -r };
            meshBuffer->Position(1 + index) = Vector3<float>{ x1, +r, -r };
            meshBuffer->Position(2 + index) = Vector3<float>{ x1, +r, +r };
            meshBuffer->Position(3 + index) = Vector3<float>{ x1, -r, +r };

            meshBuffer->Normal(0 + index) = Vector3<float>{ +1, 0, 0 };
            meshBuffer->Normal(1 + index) = Vector3<float>{ +1, 0, 0 };
            meshBuffer->Normal(2 + index) = Vector3<float>{ +1, 0, 0 };
            meshBuffer->Normal(3 + index) = Vector3<float>{ +1, 0, 0 };

            meshBuffer->TCoord(0, 0 + index) = Vector2<float>{ tex0, 1 };
            meshBuffer->TCoord(0, 1 + index) = Vector2<float>{ tex0, 0 };
            meshBuffer->TCoord(0, 2 + index) = Vector2<float>{ tex1, 0 };
            meshBuffer->TCoord(0, 3 + index) = Vector2<float>{ tex1, 1 };

            meshBuffer->Color(0, 0 + index) = c.ToArray();
            meshBuffer->Color(0, 1 + index) = c.ToArray();
            meshBuffer->Color(0, 2 + index) = c.ToArray();
            meshBuffer->Color(0, 3 + index) = c.ToArray();
            index += 4;
		};
	}

	for (int i = 0; i < resolutionY; ++i) 
    {
		float pixelPosY = i * pixelSizeY - 0.5f;
		float y0 = -pixelPosY - pixelSizeY;
		float y1 = -pixelPosY;
		float tex0 = (i + 0.1f) * pixelSizeY;
		float tex1 = (i + 0.9f) * pixelSizeY;
        
        {
            // y-
            meshBuffer->Position(0 + index) = Vector3<float>{ -r, y0, -r };
            meshBuffer->Position(1 + index) = Vector3<float>{ +r, y0, -r };
            meshBuffer->Position(2 + index) = Vector3<float>{ +r, y0, +r };
            meshBuffer->Position(3 + index) = Vector3<float>{ -r, y0, +r };

            meshBuffer->Normal(0 + index) = Vector3<float>{ 0, -1, 0 };
            meshBuffer->Normal(1 + index) = Vector3<float>{ 0, -1, 0 };
            meshBuffer->Normal(2 + index) = Vector3<float>{ 0, -1, 0 };
            meshBuffer->Normal(3 + index) = Vector3<float>{ 0, -1, 0 };

            meshBuffer->TCoord(0, 0 + index) = Vector2<float>{ 0, tex0 };
            meshBuffer->TCoord(0, 1 + index) = Vector2<float>{ 1, tex0 };
            meshBuffer->TCoord(0, 2 + index) = Vector2<float>{ 1, tex1 };
            meshBuffer->TCoord(0, 3 + index) = Vector2<float>{ 0, tex1 };

            meshBuffer->Color(0, 0 + index) = c.ToArray();
            meshBuffer->Color(0, 1 + index) = c.ToArray();
            meshBuffer->Color(0, 2 + index) = c.ToArray();
            meshBuffer->Color(0, 3 + index) = c.ToArray();
            index += 4;
        }

        {
			// y+
            meshBuffer->Position(0 + index) = Vector3<float>{ -r, y1, -r };
            meshBuffer->Position(1 + index) = Vector3<float>{ -r, y1, +r };
            meshBuffer->Position(2 + index) = Vector3<float>{ +r, y1, +r };
            meshBuffer->Position(3 + index) = Vector3<float>{ +r, y1, -r };

            meshBuffer->Normal(0 + index) = Vector3<float>{ 0, +1, 0 };
            meshBuffer->Normal(1 + index) = Vector3<float>{ 0, +1, 0 };
            meshBuffer->Normal(2 + index) = Vector3<float>{ 0, +1, 0 };
            meshBuffer->Normal(3 + index) = Vector3<float>{ 0, +1, 0 };

            meshBuffer->TCoord(0, 0 + index) = Vector2<float>{ 0, tex0 };
            meshBuffer->TCoord(0, 1 + index) = Vector2<float>{ 0, tex1 };
            meshBuffer->TCoord(0, 2 + index) = Vector2<float>{ 1, tex1 };
            meshBuffer->TCoord(0, 3 + index) = Vector2<float>{ 1, tex0 };

            meshBuffer->Color(0, 0 + index) = c.ToArray();
            meshBuffer->Color(0, 1 + index) = c.ToArray();
            meshBuffer->Color(0, 2 + index) = c.ToArray();
            meshBuffer->Color(0, 3 + index) = c.ToArray();
            index += 4;
		}
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

	// Create mesh object
	std::shared_ptr<BaseMesh> mesh(new NormalMesh());
	mesh->AddMeshBuffer(meshBuffer);
	ScaleMesh(mesh, scale);  // also recalculates bounding box
	return mesh;
}

/*
    Caches extrusion meshes so that only one of them per resolution
    is needed. Also caches one cube (for convenience).

    E.g. there is a single extrusion mesh that is used for all
    16x16 px images, another for all 256x256 px images, and so on.

    WARNING: Not thread safe. This should not be a problem since
    rendering related classes (such as WieldMeshNode) will be
    used from the rendering thread only.
*/
class ExtrusionMeshCache
{
public:
    // Constructor
    ExtrusionMeshCache()
    {
        for (int resolution = MIN_EXTRUSION_MESH_RESOLUTION;
            resolution <= MAX_EXTRUSION_MESH_RESOLUTION;
            resolution *= 2)
        {
            mExtrusionMeshes[resolution] =
                CreateExtrusionMesh(resolution, resolution);
        }
        mCube = CreateCubeMesh(Vector3<float>{1.0, 1.0, 1.0});
    }
    // Destructor
    virtual ~ExtrusionMeshCache()
    {
        mExtrusionMeshes.clear();
    }
    // Get closest extrusion mesh for given image dimensions
    // Caller must drop the returned pointer
    std::shared_ptr<BaseMesh> Create(Vector2<unsigned int> dim)
    {
        // handle non-power of two textures inefficiently without cache
        if (!IsPowerOfTwo(dim[0]) || !IsPowerOfTwo(dim[1]))
            return CreateExtrusionMesh(dim[0], dim[1]);

        int maxdim = std::max(dim[0], dim[1]);

        std::map<int, std::shared_ptr<BaseMesh>>::iterator it = mExtrusionMeshes.lower_bound(maxdim);
        if (it == mExtrusionMeshes.end())
        {
            // no viable resolution found; use largest one
            it = mExtrusionMeshes.find(MAX_EXTRUSION_MESH_RESOLUTION);
            LogAssert(it != mExtrusionMeshes.end(), "invalid");
        }

        std::shared_ptr<BaseMesh> mesh = it->second;
        return mesh;
    }
    // Returns a 1x1x1 cube mesh with one meshbuffer (material) per face
    // Caller must drop the returned pointer
    std::shared_ptr<BaseMesh> CreateCube()
    {
        return mCube;
    }

private:
    std::map<int, std::shared_ptr<BaseMesh>> mExtrusionMeshes;
    std::shared_ptr<BaseMesh> mCube;
};

// create a cache for extrusion meshes (and a cube mesh)
std::shared_ptr<ExtrusionMeshCache> ExtrusionCache = std::make_shared<ExtrusionMeshCache>();

WieldMeshNode::WieldMeshNode(const ActorId actorId, bool lighting, 
	VisualEnvironment* env, PVWUpdater* updater) : Node(actorId, NT_MESH), 
	mEnvironment(env), mMaterialType(MT_TRANSPARENT_ALPHA_CHANNEL_REF), mLighting(lighting)
{
	mEnableShaders = Settings::Get()->GetBool("enable_shaders");
	mAnisotropicFilter = Settings::Get()->GetBool("anisotropic_filter");
	mBilinearFilter = Settings::Get()->GetBool("bilinear_filter");
	mTrilinearFilter = Settings::Get()->GetBool("trilinear_filter");

	// Disable bounding box culling for this scene node
	// since we won't calculate the bounding box.
    SetCullingMode(CULL_NEVER);

	mPVWUpdater = updater;

	mRasterizerState = std::make_shared<RasterizerState>();
}

WieldMeshNode::~WieldMeshNode()
{
	for (auto const& visual : mVisuals)
		mPVWUpdater->Unsubscribe(visual->GetEffect()->GetPVWMatrixConstant());
}

void WieldMeshNode::CreateMesh()
{
    // Create the child scene node
    SetMesh(ExtrusionCache->CreateCube());

	SetReadOnlyMaterials(false);
	SetVisible(false);
}


//! Sets a new mesh
void WieldMeshNode::SetMesh(const std::shared_ptr<BaseMesh>& mesh)
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

			unsigned int shaderId =
				mEnvironment->GetShaderSource()->GetShader("Object", TILE_MATERIAL_BASIC, NDT_NORMAL);
			ShaderInfo shader = mEnvironment->GetShaderSource()->GetShaderInfo(shaderId);
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


void WieldMeshNode::SetCube(const ContentFeatures& f, Vector3<float> wieldScale)
{
	std::shared_ptr<BaseMesh> cubemesh = ExtrusionCache->CreateCube();
    std::shared_ptr<BaseMesh> copy = CloneMesh(cubemesh);

	PostProcessNodeMesh(copy, f, false, true, &mMaterialType, &mColors, true);
	ChangeToMesh(copy);
	GetRelativeTransform().SetScale(wieldScale * (float)WIELD_SCALE_FACTOR);
}

void WieldMeshNode::SetExtruded(const std::string& imageName, const std::string& overlayName, 
    Vector3<float> wieldScale, BaseTextureSource* texSrc, uint16_t numFrames)
{
	std::shared_ptr<Texture2> texture = texSrc->GetTexture(imageName);
	if (!texture) 
    {
		ChangeToMesh(nullptr);
		return;
	}

	std::shared_ptr<Texture2> overlayTexture =
		overlayName.empty() ? NULL : texSrc->GetTexture(overlayName);

    Vector2<unsigned int> dim{ texture->GetDimension(0),  texture->GetDimension(1) };
	// Detect animation texture and pull off top frame instead of using entire thing
	if (numFrames > 1) 
    {
		unsigned int frameHeight = dim[1] / numFrames;
        dim = Vector2<unsigned int>{ dim[0], frameHeight };
	}
	std::shared_ptr<BaseMesh> original = ExtrusionCache->Create(dim);
    std::shared_ptr<BaseMesh> mesh = CloneMesh(original);

	//set texture
	mesh->GetMeshBuffer(0)->GetMaterial()->SetTexture(TT_DIFFUSE, texSrc->GetTexture(imageName));
	if (overlayTexture) 
    {
        BaseMeshBuffer* copy = CloneMeshBuffer(mesh->GetMeshBuffer(0));
		copy->GetMaterial()->SetTexture(TT_DIFFUSE, overlayTexture);
		mesh->AddMeshBuffer(copy);
	}
	RecalculateBoundingBox(mesh);

	ChangeToMesh(mesh);

	GetRelativeTransform().SetScale(wieldScale * (float)WIELD_SCALE_FACTOR_EXTRUDED);

	// Customize materials
	for (unsigned int layer = 0; layer < GetMaterialCount(); layer++) 
    {
		std::shared_ptr<Material> material = GetMaterial(layer);
        material->mTextureLayer[0].mModeU = SamplerState::CLAMP;
		material->mTextureLayer[0].mModeV = SamplerState::CLAMP;
		material->mType = mMaterialType;
		material->mTypeParam = 0.5f;
        material->mCullMode = RasterizerState::CULL_NONE; //RasterizerState::CULL_BACK;

		if (material->IsTransparent())
		{
			material->mBlendTarget.enable = true;
			material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
			material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;
		}
		else
		{
			material->mBlendTarget.enable = true;
			material->mBlendTarget.srcColor = BlendState::BM_ONE;
			material->mBlendTarget.dstColor = BlendState::BM_ZERO;
			material->mBlendTarget.srcAlpha = BlendState::BM_ONE;
			material->mBlendTarget.dstAlpha = BlendState::BM_ZERO;
		}

		// Enable bi/trilinear filtering only for high resolution textures
        material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
		if (dim[0] > 32) 
        {
            //bilinear interpolation (no mipmapping)
            if (mBilinearFilter)
                material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
            //trilinear interpolation (mipmapping)
            if (mTrilinearFilter)
                material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
		} 
        else
        {
            // mipmaps cause "thin black line" artifacts
            material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
        }

        if (mAnisotropicFilter)
            material->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;

		if (mEnableShaders)
			material->SetTexture(TT_COUNT, texSrc->GetShaderFlagsTexture(false));
	}
}

static std::shared_ptr<BaseMesh> CreateSpecialNodeMesh(MapNode node,
	std::vector<ItemPartColor>* colors, const ContentFeatures& cFeatures, VisualEnvironment* env)
{
	MeshMakeData meshMakeData(env, false);
	meshMakeData.SetSmoothLighting(false);

    MeshCollector collector;
	MapblockMeshGenerator gen(&meshMakeData, &collector);

	if (node.GetParam2()) 
    {
		// keep it
	} 
    else if (cFeatures.paramType2 == CPT2_WALLMOUNTED ||
        cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED)
    {
        if (cFeatures.drawType == NDT_TORCHLIKE)
        {
            node.SetParam2(1);
        }
        else if (cFeatures.drawType == NDT_SIGNLIKE ||
            cFeatures.drawType == NDT_NODEBOX ||
            cFeatures.drawType == NDT_MESH)
        {
            node.SetParam2(4);
        }
	}
	gen.RenderSingle(node.GetContent(), node.GetParam2());

	colors->clear();
	std::shared_ptr<BaseMesh> mesh(new NormalMesh());
    for (auto& prebuffers : collector.prebuffers)
    {
        for (PreMeshBuffer& p : prebuffers)
        {
            if (p.layer.materialFlags & MATERIAL_FLAG_ANIMATION) 
            {
                const FrameSpec &frame = (*p.layer.frames)[0];
                p.layer.texture = frame.texture;
                p.layer.normalTexture = frame.normalTexture;
            }
            for (Vertex& v : p.vertices) 
                v.color[3] = 1.0f; //v.Color.SetAlpha(255)

			VertexFormat vformat;
			vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
			vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
			vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);
			vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);
			MeshBuffer* buf = new MeshBuffer(vformat, 
				(unsigned int)p.vertices.size(), (unsigned int)p.indices.size() / 3, sizeof(unsigned int));

            // fill vertices
            Vertex* vertex = buf->GetVertice()->Get<Vertex>();
            for (unsigned int i = 0; i < p.vertices.size(); i++)
            {
                vertex[i].position = p.vertices[i].position;
                vertex[i].normal = p.vertices[i].normal;
                vertex[i].tcoord = p.vertices[i].tcoord;
                vertex[i].color = p.vertices[i].color;
            }

            //fill indices
            unsigned int idx = 0;
            for (unsigned int i = 0; i < p.indices.size(); i+=3)
            {
                buf->GetIndice()->SetTriangle(idx++,
                    p.indices[i], p.indices[i + 1], p.indices[i + 2]);
            }

            buf->GetMaterial()->SetTexture(TT_DIFFUSE, p.layer.texture);
            p.layer.ApplyMaterialOptions(buf->GetMaterial());
            mesh->AddMeshBuffer(buf);

            colors->push_back(ItemPartColor(p.layer.hasColor, p.layer.color));
        }
    }
	RecalculateBoundingBox(mesh);

	return mesh;
}

void WieldMeshNode::SetItem(const ItemStack& itemStack, bool checkWieldImage)
{
    BaseShaderSource* shdrSrc = mEnvironment->GetShaderSource();
    BaseTextureSource* texSrc = mEnvironment->GetTextureSource();
    BaseItemManager* itemMgr = mEnvironment->GetItemManager();
    const NodeManager* nodeMgr = mEnvironment->GetNodeManager();
	const Item& item = itemStack.GetDefinition(itemMgr);
	const ContentFeatures& cFeatures = nodeMgr->Get(item.name);
	uint16_t id = nodeMgr->GetId(item.name);

	std::shared_ptr<BaseMesh> mesh = nullptr;
	if (mEnableShaders) 
    {
		unsigned int shaderId = shdrSrc->GetShader("Object", TILE_MATERIAL_BASIC, NDT_NORMAL);
		mMaterialType = shdrSrc->GetShaderInfo(shaderId).material;
	}

	// Color-related
	mColors.clear();
	mBaseColor = itemMgr->GetItemstackColor(itemStack, mEnvironment);

	// If wieldImage needs to be checked and is defined, it overrides everything else
	if (!item.wieldImage.empty() && checkWieldImage) 
    {
		SetExtruded(item.wieldImage, item.wieldOverlay, item.wieldScale, texSrc, 1);
		mColors.emplace_back();
		// overlay is white, if present
		mColors.emplace_back(true, SColor(0xFFFFFFFF));
		return;
	}

	// Handle nodes
	// See also CItemDefManager::CreateInventoryCached()
	if (item.type == ITEM_NODE) 
    {
		bool cullBackface = cFeatures.NeedsBackfaceCulling();

		// Select rendering method
		switch (cFeatures.drawType)
        {
		    case NDT_AIRLIKE:
			    SetExtruded("no_texture_airlike.png", "",
                    Vector3<float>{1.0, 1.0, 1.0}, texSrc, 1);
			    break;
		    case NDT_SIGNLIKE:
		    case NDT_TORCHLIKE:
		    case NDT_RAILLIKE:
		    case NDT_PLANTLIKE:
		    case NDT_FLOWINGLIQUID: 
            {
			    Vector3<float> wscale = item.wieldScale;
			    if (cFeatures.drawType == NDT_FLOWINGLIQUID)
				    wscale[2] *= 0.1f;
			    SetExtruded(
                    texSrc->GetTextureName(cFeatures.tiles[0].layers[0].textureId),
				    texSrc->GetTextureName(cFeatures.tiles[0].layers[1].textureId),
				    wscale, texSrc, cFeatures.tiles[0].layers[0].animationFrameCount);
			    // Add color
			    const TileLayer& l0 = cFeatures.tiles[0].layers[0];
			    mColors.emplace_back(l0.hasColor, l0.color);
			    const TileLayer& l1 = cFeatures.tiles[0].layers[1];
			    mColors.emplace_back(l1.hasColor, l1.color);
			    break;
		    }
		    case NDT_PLANTLIKE_ROOTED: 
            {
			    SetExtruded(texSrc->GetTextureName(cFeatures.specialTiles[0].layers[0].textureId),
				    "", item.wieldScale, texSrc, cFeatures.specialTiles[0].layers[0].animationFrameCount);
			    // Add color
			    const TileLayer& l0 = cFeatures.specialTiles[0].layers[0];
			    mColors.emplace_back(l0.hasColor, l0.color);
			    break;
		    }
		    case NDT_NORMAL:
		    case NDT_ALLFACES:
		    case NDT_LIQUID:
			    SetCube(cFeatures, item.wieldScale);
			    break;
		    default: 
            {
			    // Render non-trivial drawtypes like the actual node
			    MapNode n(id);
			    n.SetParam2(item.placeParam2);

			    mesh = CreateSpecialNodeMesh(n, &mColors, cFeatures, mEnvironment);
			    ChangeToMesh(mesh);
			    GetRelativeTransform().SetScale(
				    item.wieldScale * (float)WIELD_SCALE_FACTOR / (BS * cFeatures.visualScale));
			    break;
		    }
		}

		unsigned int materialCount = (unsigned int)GetMaterialCount();
		for (unsigned int i = 0; i < materialCount; ++i)
        {
			const std::shared_ptr<Material>& material = GetMaterial(i);
			material->mType = mMaterialType;
			material->mTypeParam = 0.5f;

			if (material->IsTransparent())
			{
				material->mBlendTarget.enable = true;
				material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
				material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
				material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
				material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;
			}
			else
			{
				material->mBlendTarget.enable = true;
				material->mBlendTarget.srcColor = BlendState::BM_ONE;
				material->mBlendTarget.dstColor = BlendState::BM_ZERO;
				material->mBlendTarget.srcAlpha = BlendState::BM_ONE;
				material->mBlendTarget.dstAlpha = BlendState::BM_ZERO;
			}

			material->mCullMode = RasterizerState::CULL_NONE;
				//cullBackface ? RasterizerState::CULL_BACK : RasterizerState::CULL_NONE;
            //bilinear interpolation (no mipmapping)
            if (mBilinearFilter)
                material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_P;
            //trilinear interpolation (mipmapping)
            if (mTrilinearFilter)
                material->mTextureLayer[0].mFilter = SamplerState::Filter::MIN_L_MAG_L_MIP_L;
		}
		return;
	} 
    else if (!item.inventoryImage.empty()) 
    {
		SetExtruded(item.inventoryImage, item.inventoryOverlay, item.wieldScale, texSrc, 1);
		mColors.emplace_back();
		// overlay is white, if present
		mColors.emplace_back(true, SColor(0xFFFFFFFF));
		return;
	}

	// no wield mesh found
	ChangeToMesh(nullptr);
}

void WieldMeshNode::SetColor(SColor c)
{
	LogAssert(!mLighting, "invalid lighting");
	std::shared_ptr<BaseMesh> mesh = GetMesh();
	if (!mesh)
		return;

	uint8_t red = c.GetRed();
	uint8_t green = c.GetGreen();
	uint8_t blue = c.GetBlue();
	unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
	for (unsigned int j = 0; j < mc; j++) 
    {
		SColor bc(mBaseColor);
		if ((mColors.size() > j) && (mColors[j].overrideBase))
			bc = mColors[j].color;
		SColor bufferColor(255,
			bc.GetRed() * red / 255,
			bc.GetGreen() * green / 255,
			bc.GetBlue() * blue / 255);

        std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(j);
		if (mEnableShaders)
			SetMeshBufferColor(buf, bufferColor);
		else
			ColorizeMeshBuffer(buf, &bufferColor);
	}
}

void WieldMeshNode::SetNodeLightColor(SColor color)
{
	if (mEnableShaders)
    {
		for (unsigned int i = 0; i < GetMaterialCount(); ++i)
        {
			const std::shared_ptr<Material>& material = GetMaterial(i);
			material->mEmissive = SColorF(color).ToArray();
		}
	}

	SetColor(color);
}

//! prerender
bool WieldMeshNode::PreRender(Scene* pScene)
{
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
bool WieldMeshNode::Render(Scene* pScene)
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

void WieldMeshNode::UpdateShaderConstants(unsigned int idx, Scene* pScene)
{
	std::shared_ptr<Visual> visual = GetVisual(idx);
	if (visual)
	{
		std::shared_ptr<ObjectEffect> effect =
			std::dynamic_pointer_cast<ObjectEffect>(visual->GetEffect());
		if (!effect)
			return;

		if (mCullMode == CullingMode::CULL_NEVER)
		{
			Matrix4x4<float> wMatrix = GetAbsoluteTransform().GetHMatrix();
			effect->SetWMatrix(wMatrix);
			Renderer::Get()->Update(effect->GetWMatrixConstant());

			effect->SetVWMatrix(wMatrix);
			Renderer::Get()->Update(effect->GetVWMatrixConstant());

			Matrix4x4<float> pvMatrix = pScene->GetActiveCamera()->Get()->GetProjectionMatrix();
			effect->SetPVMatrix(pvMatrix);
			Renderer::Get()->Update(effect->GetPVWMatrixConstant());

			Matrix4x4<float> pvwMatrix = pvMatrix * wMatrix;
			effect->SetPVWMatrix(pvwMatrix);
			Renderer::Get()->Update(effect->GetPVWMatrixConstant());
		}
		else
		{
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
		}

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


void WieldMeshNode::ChangeToMesh(std::shared_ptr<BaseMesh> mesh)
{
	if (!mesh) 
    {
        std::shared_ptr<BaseMesh> dummyMesh = ExtrusionCache->CreateCube();
		SetVisible(false);
		SetMesh(dummyMesh);
	} 
    else SetMesh(mesh);

    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
    {
        const std::shared_ptr<Material>& material = GetMaterial(i);
        material->mLighting = mLighting;
    }
    for (unsigned int i = 0; i < GetVisualCount(); ++i)
    {
        // need to normalize normals when lighting is enabled (because of SetScale())
		GetVisual(i)->UpdateModelBound();
        if (mLighting)
            GetVisual(i)->UpdateModelNormals();
    }

	SetVisible(true);
}

void GetItemMesh(const ItemStack& itemStack, ItemMesh* result, VisualEnvironment* env)
{
    BaseShaderSource* shdrSrc = env->GetShaderSource();
    BaseTextureSource* texSrc = env->GetTextureSource();
    BaseItemManager* itemMgr = env->GetItemManager();
    const NodeManager* nodeMgr = env->GetNodeManager();
    const Item& item = itemStack.GetDefinition(itemMgr);
    const ContentFeatures& cFeatures = nodeMgr->Get(item.name);
    uint16_t id = nodeMgr->GetId(item.name);

	LogAssert(ExtrusionCache, "Extrusion mesh cache is not yet initialized");
	
	std::shared_ptr<BaseMesh> mesh = nullptr;

	// Shading is on by default
	result->needsShading = true;

	bool cullBackface = cFeatures.NeedsBackfaceCulling();

	// If inventoryImage is defined, it overrides everything else
	if (!item.inventoryImage.empty()) 
    {
		mesh = GetExtrudedMesh(texSrc, item.inventoryImage, item.inventoryOverlay);
		result->bufferColors.emplace_back();
		// overlay is white, if present
		result->bufferColors.emplace_back(true, SColor(0xFFFFFFFF));
		// Items with inventory images do not need shading
		result->needsShading = false;
	} 
    else if (item.type == ITEM_NODE && cFeatures.drawType == NDT_AIRLIKE)
    {
		// Fallback image for airlike node
		mesh = GetExtrudedMesh(texSrc, "no_texture_airlike.png", item.inventoryOverlay);
		result->needsShading = false;
	} 
    else if (item.type == ITEM_NODE) 
    {
		switch (cFeatures.drawType)
        {
		    case NDT_NORMAL:
		    case NDT_ALLFACES:
		    case NDT_LIQUID:
		    case NDT_FLOWINGLIQUID: 
            {
			    std::shared_ptr<BaseMesh> cube = ExtrusionCache->CreateCube();
			    mesh = CloneMesh(cube);

			    if (cFeatures.drawType == NDT_FLOWINGLIQUID)
                {
                    ScaleMesh(mesh, Vector3<float>{1.2f, 0.03f, 1.2f});
                    TranslateMesh(mesh, Vector3<float>{0, -0.57f, 0});
			    } 
                else ScaleMesh(mesh, Vector3<float>{1.2f, 1.2f, 1.2f});

			    // add overlays
			    PostProcessNodeMesh(mesh, cFeatures, false, false, nullptr, &result->bufferColors, true);
			    if (cFeatures.drawType == NDT_ALLFACES)
                    ScaleMesh(mesh, Vector3<float>{cFeatures.visualScale, cFeatures.visualScale, cFeatures.visualScale});
			    break;
		    }
		    case NDT_PLANTLIKE: 
            {
			    mesh = GetExtrudedMesh(texSrc,
				    texSrc->GetTextureName(cFeatures.tiles[0].layers[0].textureId),
				    texSrc->GetTextureName(cFeatures.tiles[0].layers[1].textureId));
			    // Add color
			    const TileLayer& l0 = cFeatures.tiles[0].layers[0];
			    result->bufferColors.emplace_back(l0.hasColor, l0.color);
			    const TileLayer& l1 = cFeatures.tiles[0].layers[1];
			    result->bufferColors.emplace_back(l1.hasColor, l1.color);
			    break;
		    }
		    case NDT_PLANTLIKE_ROOTED: 
            {
			    mesh = GetExtrudedMesh(texSrc,
				    texSrc->GetTextureName(cFeatures.specialTiles[0].layers[0].textureId), "");
			    // Add color
			    const TileLayer& l0 = cFeatures.specialTiles[0].layers[0];
			    result->bufferColors.emplace_back(l0.hasColor, l0.color);
			    break;
		    }
		    default: 
            {
			    // Render non-trivial drawtypes like the actual node
			    MapNode n(id);
			    n.SetParam2(item.placeParam2);

			    mesh = CreateSpecialNodeMesh(n, &result->bufferColors, cFeatures, env);
                ScaleMesh(mesh, Vector3<float>{0.12f, 0.12f, 0.12f});
			    break;
		    }
		}

		unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
		for (unsigned int i = 0; i < mc; ++i) 
        {
			std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(i);
			std::shared_ptr<Material>& material = buf->GetMaterial();
			material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
			material->mTypeParam = 0.5f;

			material->mBlendTarget.enable = true;
			material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
			material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
			material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

            material->mLighting = false;
			material->mFillMode = RasterizerState::FILL_SOLID;
			material->mCullMode = RasterizerState::CULL_NONE;
				//cullBackface ? RasterizerState::CULL_BACK : RasterizerState::CULL_NONE;
            material->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
		}

		RotateMeshXZBy(mesh, -45);
		RotateMeshYZBy(mesh, -30);
	}
	result->mesh = mesh;
}

std::shared_ptr<BaseMesh> GetExtrudedMesh(BaseTextureSource* texSrc,
	const std::string& imageName, const std::string& overlayName)
{
	// check textures
	std::shared_ptr<Texture2> texture = texSrc->GetTextureForMesh(imageName);
	if (!texture)
		return NULL;

	std::shared_ptr<Texture2> overlayTexture =
		(overlayName.empty()) ? NULL : texSrc->GetTexture(overlayName);

	// get mesh
    Vector2<unsigned int> dim{ texture->GetDimension(1), texture->GetDimension(2) };
	std::shared_ptr<BaseMesh> original = ExtrusionCache->Create(dim);
	std::shared_ptr<BaseMesh> mesh = CloneMesh(original);

	//set texture
	mesh->GetMeshBuffer(0)->GetMaterial()->SetTexture(TT_DIFFUSE, texSrc->GetTexture(imageName));
	if (overlayTexture) 
    {
		BaseMeshBuffer* copy = CloneMeshBuffer(mesh->GetMeshBuffer(0));
		copy->GetMaterial()->SetTexture(TT_DIFFUSE, overlayTexture);
		mesh->AddMeshBuffer(copy);
	}
	RecalculateBoundingBox(mesh);

	// Customize materials
	for (unsigned int layer = 0; layer < mesh->GetMeshBufferCount(); layer++) 
    {
		std::shared_ptr<Material>& material = mesh->GetMeshBuffer(layer)->GetMaterial();
        material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;
        material->mTypeParam = 0.5f;

		material->mBlendTarget.enable = true;
		material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
		material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
		material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
		material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

        material->mTextureLayer[0].mModeU = SamplerState::Mode::CLAMP;
		material->mTextureLayer[0].mModeV = SamplerState::Mode::CLAMP;

        material->mLighting = false;
        material->mCullMode = RasterizerState::CULL_NONE; //RasterizerState::CULL_BACK;
        material->mTextureLayer[0].mFilter = SamplerState::Filter::ANISOTROPIC;
	}
    ScaleMesh(mesh, Vector3<float>{2.0, 2.0, 2.0});

	return mesh;
}

void PostProcessNodeMesh(std::shared_ptr<BaseMesh> mesh, 
    const ContentFeatures& cFeatures, bool useShaders, bool setMaterial, 
    const MaterialType* matType, std::vector<ItemPartColor>* colors, bool applyScale)
{
	unsigned int mc = (unsigned int)mesh->GetMeshBufferCount();
	// Allocate colors for existing buffers
	colors->clear();
	for (unsigned int i = 0; i < mc; ++i)
		colors->push_back(ItemPartColor());

	for (unsigned int i = 0; i < mc; ++i) 
    {
		const TileSpec* tile = &(cFeatures.tiles[i]);
		std::shared_ptr<BaseMeshBuffer> buf = mesh->GetMeshBuffer(i);
		for (int layernum = 0; layernum < MAX_TILE_LAYERS; layernum++) 
        {
			const TileLayer *layer = &tile->layers[layernum];
			if (layer->textureId == 0)
				continue;
			if (layernum != 0) 
            {
				BaseMeshBuffer* copy = CloneMeshBuffer(buf);
				copy->GetMaterial() = buf->GetMaterial();
				mesh->AddMeshBuffer(copy);

				buf.reset(copy);
				colors->push_back(ItemPartColor(layer->hasColor, layer->color));
			} 
            else 
            {
				(*colors)[i] = ItemPartColor(layer->hasColor, layer->color);
			}

			std::shared_ptr<Material>& material = buf->GetMaterial();
			if (setMaterial)
				layer->ApplyMaterialOptions(material);
			if (matType) 
            {
				material->mType = *matType;
			}
			if (layer->animationFrameCount > 1) 
            {
				const FrameSpec &animationFrame = (*layer->frames)[0];
				material->SetTexture(TT_DIFFUSE, animationFrame.texture);
			} 
            else 
            {
				material->SetTexture(TT_DIFFUSE, layer->texture);
			}
			if (useShaders) 
            {
				if (layer->normalTexture) 
                {
					if (layer->animationFrameCount > 1) 
                    {
						const FrameSpec &animationFrame = (*layer->frames)[0];
						material->SetTexture(TT_NORMALS, animationFrame.normalTexture);
					} 
                    else material->SetTexture(TT_NORMALS, layer->normalTexture);
				}
				material->SetTexture(TT_COUNT, layer->flagsTexture);
			}
			if (applyScale && tile->worldAligned) 
            {
				unsigned int n = buf->GetVertice()->GetNumElements();
				for (unsigned int k = 0; k != n; ++k)
					buf->TCoord(0, k) /= (float)layer->scale;
			}
		}
	}

	RecalculateBoundingBox(mesh);
}

//! returns the axis aligned bounding box of this node
BoundingBox<float>& WieldMeshNode::GetBoundingBox()
{
	return mMesh ? mMesh->GetBoundingBox() : mBoundingBox;
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
int WieldMeshNode::DetachChild(std::shared_ptr<Node> const& child)
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
std::shared_ptr<Visual> const& WieldMeshNode::GetVisual(unsigned int i)
{
	if (i >= mVisuals.size())
		return nullptr;

	return mVisuals[i];
}

//! return amount of visuals of this scene node.
size_t WieldMeshNode::GetVisualCount() const
{
	return mVisuals.size();
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& WieldMeshNode::GetMaterial(unsigned int i)
{
	if (i >= mMesh->GetMeshBufferCount())
		return nullptr;

	return mMesh->GetMeshBuffer(i)->GetMaterial();
}

//! returns amount of materials used by this scene node.
size_t WieldMeshNode::GetMaterialCount() const
{
	return mMesh->GetMeshBufferCount();
}

//! Sets the texture of the specified layer in all materials of this scene node to the new texture.
/** \param textureLayer Layer of texture to be set. Must be a value smaller than MATERIAL_MAX_TEXTURES.
\param texture New texture to be used. */
void WieldMeshNode::SetMaterialTexture(unsigned int textureLayer, std::shared_ptr<Texture2> texture)
{
	if (textureLayer >= MATERIAL_MAX_TEXTURES)
		return;

	for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		GetMaterial(i)->SetTexture(textureLayer, texture);
}

//! Sets the material type of all materials in this scene node to a new material type.
/** \param newType New type of material to be set. */
void WieldMeshNode::SetMaterialType(MaterialType newType)
{
	for (unsigned int i = 0; i < GetMaterialCount(); ++i)
		GetMaterial(i)->mType = newType;
}

//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void WieldMeshNode::SetReadOnlyMaterials(bool readonly)
{
	mReadOnlyMaterials = readonly;
}

//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool WieldMeshNode::IsReadOnlyMaterials() const
{
	return mReadOnlyMaterials;
}