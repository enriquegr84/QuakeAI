// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CloudSystemNode.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Scene/Scene.h"

#include "Core/OS/OS.h"

#define NOISE_MAGIC_X    1619
#define NOISE_MAGIC_Y    31337
#define NOISE_MAGIC_Z    52591
#define NOISE_MAGIC_SEED 1013

float ApplyEaseCurve(float t)
{
    return t * t * t * (t * (6.f * t - 15.f) + 10.f);
}

float ApplyLinearInterpolation(float v0, float v1, float t)
{
    return v0 + (v1 - v0) * t;
}

float Noise2D(int x, int y, int seed)
{
    unsigned int n = (NOISE_MAGIC_X * x + NOISE_MAGIC_Y * y
        + NOISE_MAGIC_SEED * seed) & 0x7fffffff;
    n = (n >> 13) ^ n;
    n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
    return 1.f - (float)(int)n / 0x40000000;
}

float Noise2DGradient(float x, float y, int seed, bool eased)
{
    // Calculate the integer coordinates
    int x0 = (int)floor(x);
    int y0 = (int)floor(y);
    // Calculate the remaining part of the coordinates
    float xl = x - (float)x0;
    float yl = y - (float)y0;
    // Get values for corners of square
    float v00 = Noise2D(x0, y0, seed);
    float v10 = Noise2D(x0 + 1, y0, seed);
    float v01 = Noise2D(x0, y0 + 1, seed);
    float v11 = Noise2D(x0 + 1, y0 + 1, seed);
    // Interpolate
    if (eased)
    {
        float tx = ApplyEaseCurve(xl);
        float ty = ApplyEaseCurve(yl);
        float u = ApplyLinearInterpolation(v00, v10, tx);
        float v = ApplyLinearInterpolation(v01, v11, tx);
        return ApplyLinearInterpolation(u, v, ty);
    }
    else
    {
        float u = ApplyLinearInterpolation(v00, v10, xl);
        float v = ApplyLinearInterpolation(v01, v11, xl);
        return ApplyLinearInterpolation(u, v, y);
    }
}

float Noise2DPerlin(float x, float y, int seed,
    int octaves, float persistence, bool eased)
{
    float a = 0;
    float f = 1.0;
    float g = 1.0;
    for (int i = 0; i < octaves; i++)
    {
        a += g * Noise2DGradient(x * f, y * f, seed + i, eased);
        f *= 2.0;
        g *= persistence;
    }
    return a;
}

//! constructor
CloudSystemNode::CloudSystemNode(const ActorId actorId, PVWUpdater* updater)
    : Node(actorId, NT_CLOUD), mShadow(0), mHeight(120), mDensity(0.4f), mThickness(16.f),
    mSpeed{ 0.0f, -2.0f }, mCameraOffset{ 0, 0, 0 }, mCameraInsideCloud(false)
{
    PcgRandom rand;

    mBlockSize = 10.0f;
    mCloudSize = mBlockSize * 64.0f;
    mSeed = rand.Next();
    mOrigin = Vector2<float>::Zero();

    mColorDiffuse = SColor(255, 255, 255, 255);
    mColorBright = SColor(229, 240, 240, 255);
    mColorAmbient = SColor(255, 0, 0, 0);

    mPVWUpdater = updater;
    mMeshBuffer = std::make_shared<MeshBuffer>();
    mMeshBuffer->GetMaterial()->mLighting = false;
    mMeshBuffer->GetMaterial()->mCullMode = RasterizerState::CULL_BACK;
    mMeshBuffer->GetMaterial()->mType = MaterialType::MT_TRANSPARENT_ALPHA_CHANNEL;

    mBlendState = std::make_shared<BlendState>();
    mDepthStencilState = std::make_shared<DepthStencilState>();
    mRasterizerState = std::make_shared<RasterizerState>();
}

//! destructor
CloudSystemNode::~CloudSystemNode()
{
    mPVWUpdater->Unsubscribe(mVisual->GetEffect()->GetPVWMatrixConstant());
}

//! Set effect for rendering.
void CloudSystemNode::SetEffect(unsigned short radius, bool enable3D)
{
    mRadius = radius;
    mEnable3D = enable3D;

    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    MeshBuffer* meshBuffer = new MeshBuffer(
        vformat, mRadius * mRadius * 16, mRadius * mRadius * 8, sizeof(unsigned int));
    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
        meshBuffer->GetMaterial() = GetMaterial(i);
    mMeshBuffer.reset(meshBuffer);

    std::vector<std::string> path;
#if defined(_OPENGL_)
    path.push_back("Effects/ColorEffectVS.glsl");
    path.push_back("Effects/ColorEffectPS.glsl");
#else
    path.push_back("Effects/ColorEffectVS.hlsl");
    path.push_back("Effects/ColorEffectPS.hlsl");
#endif
    std::shared_ptr<ResHandle> resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

    const std::shared_ptr<ShaderResourceExtraData>& extra =
        std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
    if (!extra->GetProgram())
        extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    std::shared_ptr<ColorEffect> effect = std::make_shared<ColorEffect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));
    mEffect = effect;
    mVisual = std::make_shared<Visual>(
        mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), mEffect);
    mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());
}

//! prerender
bool CloudSystemNode::PreRender(Scene *pScene)
{
    if (IsVisible())
    {
        UpdateBuffers();

        pScene->AddToRenderQueue(RP_TRANSPARENT, shared_from_this());
    }

    return Node::PreRender(pScene);
}

//
// CloudSceneNode::Render					- Chapter 16, page 550
//
bool CloudSystemNode::Render(Scene *pScene)
{
    if (!Renderer::Get())
        return false;

    if (mShadow)
        mShadow->UpdateShadowVolumes(pScene);

    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
    {
        GetMaterial(i)->mCullMode = mEnable3D ?
            RasterizerState::CULL_BACK : RasterizerState::CULL_NONE;

        if (GetMaterial(i)->Update(mBlendState))
            Renderer::Get()->Unbind(mBlendState);
        if (GetMaterial(i)->Update(mDepthStencilState))
            Renderer::Get()->Unbind(mDepthStencilState);
        if (GetMaterial(i)->Update(mRasterizerState))
            Renderer::Get()->Unbind(mRasterizerState);
    }

    Renderer::Get()->SetBlendState(mBlendState);
    Renderer::Get()->SetDepthStencilState(mDepthStencilState);
    Renderer::Get()->SetRasterizerState(mRasterizerState);

    Renderer* renderer = Renderer::Get();
    renderer->Update(mVisual->GetVertexBuffer());
    renderer->Draw(mVisual);

    Renderer::Get()->SetDefaultBlendState();
    Renderer::Get()->SetDefaultDepthStencilState();
    Renderer::Get()->SetDefaultRasterizerState();

    return Node::Render(pScene);
}

//! returns the axis aligned bounding box of this node
BoundingBox<float>& CloudSystemNode::GetBoundingBox()
{
    return mBoundingBox;
}

void CloudSystemNode::Update(float deltaMs)
{
    mOrigin += deltaMs / 1000.f * mBlockSize * mSpeed;
}

void CloudSystemNode::Update(const Vector3<float>& cameraPos, SColorF color)
{
    mCameraPosition = cameraPos;
    mColorDiffuse.mRed = std::clamp(color.mRed * mColorBright.mRed, mColorAmbient.mRed, 1.f);
    mColorDiffuse.mGreen = std::clamp(color.mGreen * mColorBright.mGreen, mColorAmbient.mGreen, 1.f);
    mColorDiffuse.mBlue = std::clamp(color.mBlue * mColorBright.mBlue, mColorAmbient.mBlue, 1.f);
    mColorDiffuse.mAlpha = mColorBright.mAlpha;

    // is the camera inside the cloud mesh?
    mCameraInsideCloud = false; // default
    if (mEnable3D)
    {
        float cameraHeight = cameraPos[1] - mBlockSize * mCameraOffset[1];
        if (cameraHeight >= mBoundingBox.mMinEdge[1] && cameraHeight <= mBoundingBox.mMaxEdge[1])
        {
            Vector2<float> cameraInNoise;
            cameraInNoise[0] = floor((cameraPos[0] - mOrigin[0]) / mCloudSize + 0.5f);
            cameraInNoise[1] = floor((cameraPos[2] - mOrigin[1]) / mCloudSize + 0.5f);
            bool filled = GridFilled((int)cameraInNoise[0], (int)cameraInNoise[1]);
            mCameraInsideCloud = filled;
        }
    }
}

void CloudSystemNode::UpdateBuffers()
{
    if (mDensity <= 0.f)
        return; // no need to do anything

    int numFacesToDraw = mEnable3D ? 6 : 1;

    //Clouds move from Z+ towards Z-
    const float cloudFullRadius = mCloudSize * mRadius;

    Vector2<float> cameraPos2D{ mCameraPosition[0], mCameraPosition[2] };
    // Position of cloud noise origin from the camera
    Vector2<float> cloudOriginFromCamera = mOrigin - cameraPos2D;
    // The center point of drawing in the noise
    Vector2<int> centerDrawingInNoise({
        (int)floor(-cloudOriginFromCamera[0] / mCloudSize),
        (int)floor(-cloudOriginFromCamera[1] / mCloudSize)
    });

    // The world position of the integer center point of drawing in the noise
    Vector2<float> worldCenterDrawingInNoise = Vector2<float>({
        centerDrawingInNoise[0] * mCloudSize,
        centerDrawingInNoise[1] * mCloudSize
    }) + mOrigin;

    SColorF cloudTop = mColorDiffuse;
    SColorF cloudFrontBack = SColorF(
        mColorDiffuse.mRed * 0.95f,
        mColorDiffuse.mGreen * 0.95f,
        mColorDiffuse.mBlue * 0.95f,
        mColorDiffuse.mAlpha);
    SColorF cloudLeftRight = SColorF(
        mColorDiffuse.mRed * 0.90f,
        mColorDiffuse.mGreen * 0.90f,
        mColorDiffuse.mBlue * 0.90f,
        mColorDiffuse.mAlpha);
    SColorF cloudBottom = SColorF(
        mColorDiffuse.mRed * 0.80f,
        mColorDiffuse.mGreen * 0.80f,
        mColorDiffuse.mBlue * 0.80f,
        mColorDiffuse.mAlpha);

    // Read noise
    struct Vertex
    {
        Vector3<float> position;
        Vector4<float> color;
        Vector3<float> normal;
    } vertex[4];
    std::vector<bool> grid(mRadius * 2 * mRadius * 2);
    std::vector<Vertex> vertices;

    for (short zi = -mRadius; zi < mRadius; zi++)
    {
        unsigned short si = (zi + mRadius) * mRadius * 2 + mRadius;

        for (short xi = -mRadius; xi < mRadius; xi++)
        {
            unsigned short i = si + xi;

            grid[i] = GridFilled(
                xi + centerDrawingInNoise[0],
                zi + centerDrawingInNoise[1]
            );
        }
    }

#define GETINDEX(x, z, radius) (((z)+(radius))*(radius)*2 + (x)+(radius))
#define INAREA(x, z, radius) \
	((x) >= -(radius) && (x) < (radius) && (z) >= -(radius) && (z) < (radius))

    for (short zi0 = -mRadius; zi0 < mRadius; zi0++)
    {
        for (short xi0 = -mRadius; xi0 < mRadius; xi0++)
        {
            short zi = zi0;
            short xi = xi0;
            // Draw from back to front for proper transparency
            if (zi >= 0)
                zi = mRadius - zi - 1;
            if (xi >= 0)
                xi = mRadius - xi - 1;

            unsigned int i = GETINDEX(xi, zi, mRadius);

            if (!grid[i])
                continue;

            Vector2<float> p0 = 
                Vector2<float>{ (float)xi, (float)zi } * mCloudSize + worldCenterDrawingInNoise;

            const float rx = mCloudSize / 2.0f;
            // if clouds are flat, the top layer should be at the given height
            const float ry = mEnable3D ? mThickness * mBlockSize : 0.0f;
            const float rz = mCloudSize / 2;

            for (int i = 0; i < numFacesToDraw; i++)
            {
                switch (i)
                {
                    case 0:	// top
                        for (Vertex& v : vertex)
                        {
                            v.normal = Vector3<float>{ 0, 1, 0 };
                            v.color = cloudTop.ToArray();
                        }

                        vertex[0].position = Vector3<float>{ -rx, ry, -rz };
                        vertex[1].position = Vector3<float>{ -rx, ry, rz };
                        vertex[2].position = Vector3<float>{ rx, ry, rz };
                        vertex[3].position = Vector3<float>{ rx, ry, -rz };
                        break;
                    case 1: // back
                        if (INAREA(xi, zi - 1, mRadius)) 
                        {
                            unsigned int j = GETINDEX(xi, zi - 1, mRadius);
                            if (grid[j])
                                continue;
                        }
                        for (Vertex& v : vertex)
                        {
                            v.normal = Vector3<float>{ 0, 0, -1 };
                            v.color = cloudFrontBack.ToArray();
                        }

                        vertex[0].position = Vector3<float>{ -rx, ry, -rz };
                        vertex[1].position = Vector3<float>{ rx, ry, -rz };
                        vertex[2].position = Vector3<float>{ rx, 0, -rz };
                        vertex[3].position = Vector3<float>{ -rx, 0, -rz };
                        break;
                    case 2: //right
                        if (INAREA(xi + 1, zi, mRadius)) 
                        {
                            unsigned int j = GETINDEX(xi + 1, zi, mRadius);
                            if (grid[j])
                                continue;
                        }
                        for (Vertex& v : vertex)
                        {
                            v.normal = Vector3<float>{ 1, 0, 0 };
                            v.color = cloudLeftRight.ToArray();
                        }

                        vertex[0].position = Vector3<float>{ rx, ry, -rz };
                        vertex[1].position = Vector3<float>{ rx, ry, rz };
                        vertex[2].position = Vector3<float>{ rx, 0, rz };
                        vertex[3].position = Vector3<float>{ rx, 0, -rz };
                        break;
                    case 3: // front
                        if (INAREA(xi, zi + 1, mRadius)) 
                        {
                            unsigned int j = GETINDEX(xi, zi + 1, mRadius);
                            if (grid[j])
                                continue;
                        }
                        for (Vertex& v : vertex)
                        {
                            v.normal = Vector3<float>{ 0, 0, -1 };
                            v.color = cloudFrontBack.ToArray();
                        }

                        vertex[0].position = Vector3<float>{ rx, ry, rz };
                        vertex[1].position = Vector3<float>{ -rx, ry, rz };
                        vertex[2].position = Vector3<float>{ -rx, 0, rz };
                        vertex[3].position = Vector3<float>{ rx, 0, rz };
                        break;
                    case 4: // left
                        if (INAREA(xi - 1, zi, mRadius)) 
                        {
                            unsigned int j = GETINDEX(xi - 1, zi, mRadius);
                            if (grid[j])
                                continue;
                        }
                        for (Vertex& v : vertex)
                        {
                            v.normal = Vector3<float>{ -1, 0, 0 };
                            v.color = cloudLeftRight.ToArray();
                        }

                        vertex[0].position = Vector3<float>{ -rx, ry, rz };
                        vertex[1].position = Vector3<float>{ -rx, ry, -rz };
                        vertex[2].position = Vector3<float>{ -rx, 0, -rz };
                        vertex[3].position = Vector3<float>{ -rx, 0, rz };
                        break;
                    case 5: // bottom
                        for (Vertex& v : vertex)
                        {
                            v.normal = Vector3<float>{ 0, -1, 0 };
                            v.color = cloudBottom.ToArray();
                        }

                        vertex[0].position = Vector3<float>{ rx, 0, rz };
                        vertex[1].position = Vector3<float>{ -rx, 0, rz };
                        vertex[2].position = Vector3<float>{ -rx, 0, -rz };
                        vertex[3].position = Vector3<float>{ rx, 0, -rz };
                        break;
                }

                Vector3<float> pos{ p0[0], mHeight * mBlockSize, p0[1] };
                pos -= Vector3<float>{ 
                    mCameraOffset[0] * mBlockSize,
                    mCameraOffset[1] * mBlockSize,
                    mCameraOffset[2] * mBlockSize};

                for (Vertex& v : vertex)
                {
                    v.position += pos;
                    vertices.push_back(v);
                }
            }
        }
    }

    int quadCount = (int)vertices.size() / 4;
    std::vector<unsigned int> indices;
    for (int k = 0; k < quadCount; k++) 
    {
        indices.push_back(4 * k + 0);
        indices.push_back(4 * k + 1);
        indices.push_back(4 * k + 2);
        indices.push_back(4 * k + 2);
        indices.push_back(4 * k + 3);
        indices.push_back(4 * k + 0);
    }

    unsigned int verticeCount = (unsigned int)vertices.size();
    unsigned int primitiveCount = (unsigned int)vertices.size() / 2;
    MeshBuffer* meshBuffer = new MeshBuffer(
        mMeshBuffer->GetVertice()->GetFormat(), verticeCount, primitiveCount, sizeof(unsigned int));
    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
        meshBuffer->GetMaterial() = GetMaterial(i);
    mMeshBuffer.reset(meshBuffer);

    // fill vertices
    for (unsigned int i = 0; i < mMeshBuffer->GetVertice()->GetNumElements(); i++)
    {
        mMeshBuffer->Color(0, i) = vertices[i].color;
        mMeshBuffer->Position(i) = vertices[i].position;
    }

    // fill indices
    unsigned int idx = 0;
    for (unsigned int i = 0; i < indices.size(); i += 3)
    {
        mMeshBuffer->GetIndice()->SetTriangle(idx++,
            indices[i], indices[i + 1], indices[i + 2]);
    }

    if (mMeshBuffer)
    {
        mVisual.reset(new Visual(
            mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), mEffect));
    }

    mVisual->UpdateModelBound();
}

bool CloudSystemNode::GridFilled(int x, int y) const
{
    float cloudSizeNoise = mCloudSize / (mBlockSize * 200.f);
    float noise = Noise2DPerlin(
        (float)x * cloudSizeNoise,
        (float)y * cloudSizeNoise,
        mSeed, 3, 0.5, true);
    // normalize to 0..1 (given 3 octaves)
    const float noiseBound = 1.0f + 0.5f + 0.25f;
    float density = noise / noiseBound * 0.5f + 0.5f;
    return (density < mDensity);
}

void CloudSystemNode::UpdateBox()
{
    float heightBS = mHeight * mBlockSize;
    float thicknessBS = mThickness * mBlockSize;
    float cons = 1000000.0f;
    mBoundingBox = BoundingBox<float>(
        -mBlockSize * cons, heightBS - mBlockSize * mCameraOffset[1], -mBlockSize * cons,
        mBlockSize * cons, heightBS + thicknessBS - mBlockSize * mCameraOffset[1], mBlockSize * cons);
}

//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
int CloudSystemNode::DetachChild(std::shared_ptr<Node> const& child)
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
std::shared_ptr<Visual> const& CloudSystemNode::GetVisual(unsigned int i)
{
    return mVisual;
}

//! return amount of visuals of this scene node.
size_t CloudSystemNode::GetVisualCount() const
{
    return 1;
}

//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use GetMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
std::shared_ptr<Material> const& CloudSystemNode::GetMaterial(unsigned int i)
{
    return mMeshBuffer->GetMaterial();
}

//! returns amount of materials used by this scene node.
size_t CloudSystemNode::GetMaterialCount() const
{
    return 1;
}

//! Sets the material type of all materials in this scene node to a new material type.
/** \param newType New type of material to be set. */
void CloudSystemNode::SetMaterialType(MaterialType newType)
{
    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
        GetMaterial(i)->mType = newType;
}