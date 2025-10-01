/*
Minetest
Copyright (C) 2020 sfan5 <sfan5@live.de>

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

#include "Particles.h"

#include "Node.h"
#include "VisualEvent.h"

#include "Map/VisualMap.h"

#include "../Games/Environment/VisualEnvironment.h"

#include "../Graphics/Actors/ContentVisualActiveObject.h"

#include "../Physics/Collision.h"

#include "../Utils/Util.h"

#include "Graphic/Scene/Hierarchy/PVWUpdater.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

#include "Core/Utility/Serialize.h"

#include "Core/Threading/MutexAutolock.h"

#include "Game/GameLogic.h"

#include "Application/Settings.h"
#include "Application/GameApplication.h"


/*
    Particle
*/
ParticleNode::ParticleNode(const ActorId actorId, 
    PVWUpdater* updater, VisualEnvironment* env, VisualPlayer* player,
    const ParticleParameters& parameters, std::shared_ptr<Texture2> texture,
    Vector2<float> texpos, Vector2<float> texsize, SColor color) : 
    Node(actorId, NT_PARTICLE), mEnvironment(env)
{
    mTexpos = texpos;
    mTexsize = texsize;
    mAnimation = parameters.animation;

    // Color
    mBaseColor = color;
    mColor = color;

    // Particle related
    mPos = parameters.pos;
    mVelocity = parameters.vel;
    mAcceleration = parameters.acc;
    mExpiration = parameters.expTime;
    mPlayer = player;
    mSize = parameters.size;
    mCollisionDetection = parameters.collisionDetection;
    mCollisionRemoval = parameters.collisionRemoval;
    mObjectCollision = parameters.objectCollision;
    mVertical = parameters.vertical;
    mGlow = parameters.glow;

    const float c = Length(mPos) / 2;
    mCollisionBox = BoundingBox<float>(-c, -c, -c, c, c, c);
    SetCullingMode(CULL_NEVER);

    mPVWUpdater = updater;

    mBlendState = std::make_shared<BlendState>();
    mDepthStencilState = std::make_shared<DepthStencilState>();
    mRasterizerState = std::make_shared<RasterizerState>();

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
    };
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    MeshBuffer* meshBuffer = new MeshBuffer(vformat, 4, 2, sizeof(unsigned int));
    std::shared_ptr<Material> material = std::make_shared<Material>();
    // Texture
    material->mLighting = false;
    material->SetTexture(TT_DIFFUSE, texture);
    material->mTextureLayer[TT_DIFFUSE].mFilter = SamplerState::ANISOTROPIC;

    //mMaterial->setFlag(video::EMF_FOG_ENABLE, true);
    material->mType = MT_TRANSPARENT_ALPHA_CHANNEL;

    material->mBlendTarget.enable = true;
    material->mBlendTarget.srcColor = BlendState::BM_SRC_ALPHA;
    material->mBlendTarget.dstColor = BlendState::BM_INV_SRC_ALPHA;
    material->mBlendTarget.srcAlpha = BlendState::BM_SRC_ALPHA;
    material->mBlendTarget.dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    material->mDepthBuffer = true;
    material->mDepthMask = DepthStencilState::MASK_ALL;

    material->mFillMode = RasterizerState::FILL_SOLID;
    material->mCullMode = RasterizerState::CULL_NONE;

    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
        meshBuffer->GetMaterial() = material;

    // fill indices
    int vertices = 0;
    for (unsigned int i = 0; i < meshBuffer->GetIndice()->GetNumPrimitives(); i += 2, vertices += 4)
    {
        meshBuffer->GetIndice()->SetTriangle(i,
            (unsigned int)0 + vertices, (unsigned int)1 + vertices, (unsigned int)2 + vertices);
        meshBuffer->GetIndice()->SetTriangle(i + 1,
            (unsigned int)2 + vertices, (unsigned int)3 + vertices, (unsigned int)0 + vertices);
    }

    mMeshBuffer.reset(meshBuffer);

    std::vector<std::string> path;
#if defined(_OPENGL_)
    path.push_back("Effects/Texture2ColorEffectVS.glsl");
    path.push_back("Effects/Texture2ColorEffectPS.glsl");
#else
    path.push_back("Effects/Texture2ColorEffectVS.hlsl");
    path.push_back("Effects/Texture2ColorEffectPS.hlsl");
#endif
    std::shared_ptr<ResHandle> resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

    const std::shared_ptr<ShaderResourceExtraData>& extra =
        std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
    if (!extra->GetProgram())
        extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    std::shared_ptr<Texture2Effect> effect = std::make_shared<Texture2Effect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()), 
        mMeshBuffer->GetMaterial()->GetTexture(TT_DIFFUSE),
        SamplerState::MIN_L_MAG_L_MIP_P, SamplerState::WRAP, SamplerState::WRAP);
    mEffect = effect;
    mVisual = std::make_shared<Visual>(
        mMeshBuffer->GetVertice(), mMeshBuffer->GetIndice(), mEffect);
    mPVWUpdater->Subscribe(mWorldTransform, effect->GetPVWMatrixConstant());

    // Init lighting
    UpdateLight();

    // Init model
    UpdateVertices();
}

//! destructor
ParticleNode::~ParticleNode()
{
    mPVWUpdater->Unsubscribe(mVisual->GetEffect()->GetPVWMatrixConstant());
}

bool ParticleNode::PreRender(Scene* pScene)
{
    if (IsVisible())
        pScene->AddToRenderQueue(RP_TRANSPARENT_EFFECT, shared_from_this());

    return Node::PreRender(pScene);
}

bool ParticleNode::Render(Scene* pScene)
{
    if (!Renderer::Get())
        return false;

    for (unsigned int i = 0; i < GetMaterialCount(); ++i)
    {
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

void ParticleNode::Step(float dTime)
{
    mTime += dTime;
    if (mCollisionDetection) 
    {
        BoundingBox<float> box = mCollisionBox;
        Vector3<float> particlePos = mPos * BS;
        Vector3<float> particleVelocity = mVelocity * BS;
        CollisionMoveResult result = CollisionMoveSimple((Environment*)mEnvironment, 
            BS * 0.5f, box, 0.0f, dTime, &particlePos, &particleVelocity,
            mAcceleration * BS, nullptr, mObjectCollision);
        if (mCollisionRemoval && result.collides)
        {
            // force expiration of the particle
            mExpiration = -1.0;
        }
        else 
        {
            mPos = particlePos / BS;
            mVelocity = particleVelocity / BS;
        }
    }
    else 
    {
        mVelocity += mAcceleration * dTime;
        mPos += mVelocity * dTime;
    }

    if (mAnimation.type != TAT_NONE)
    {
        mAnimationTime += dTime;
        int frameLength, frameCount;
        const Vector2<unsigned int> texSize = Vector2<unsigned int>{
            mMeshBuffer->GetMaterial()->GetTexture(0)->GetDimension(0),
            mMeshBuffer->GetMaterial()->GetTexture(0)->GetDimension(1) };
        mAnimation.DetermineParams(texSize,  &frameCount, &frameLength, NULL);
        float frameLengthTime = frameLength / 1000.f;
        while (mAnimationTime > frameLengthTime) 
        {
            mAnimationFrame++;
            mAnimationTime -= frameLengthTime;
        }
    }

    // Update lighting
    UpdateLight();

    // Update model
    UpdateVertices();
}

void ParticleNode::UpdateLight()
{
    uint8_t light = 0;
    bool posOk;

    Vector3<short> p = Vector3<short>{
        (short)floor(mPos[0] + 0.5), (short)floor(mPos[1] + 0.5), (short)floor(mPos[2] + 0.5)};
    MapNode node = mEnvironment->GetVisualMap()->GetNode(p, &posOk);
    if (posOk)
        light = node.GetLightBlend(mEnvironment->GetDayNightRatio(), mEnvironment->GetNodeManager());
    else
        light = BlendLight(mEnvironment->GetDayNightRatio(), LIGHT_SUN, 0);

    uint8_t decodeLight = DecodeLight((uint8_t)(light + mGlow));
    mColor.Set(255,
        decodeLight * mBaseColor.GetRed() / 255,
        decodeLight * mBaseColor.GetGreen() / 255,
        decodeLight * mBaseColor.GetBlue() / 255);
}

void ParticleNode::UpdateVertices()
{
    float tx0, tx1, ty0, ty1;

    if (mAnimation.type != TAT_NONE) 
    {
        const Vector2<unsigned int> texSize = Vector2<unsigned int>{
            mMeshBuffer->GetMaterial()->GetTexture(0)->GetDimension(0),
            mMeshBuffer->GetMaterial()->GetTexture(0)->GetDimension(1) };
        Vector2<float> texCoord, frSize;
        Vector2<unsigned int> frameSize;
        texCoord = mAnimation.GetTextureCoords(texSize, mAnimationFrame);
        mAnimation.DetermineParams(texSize, NULL, NULL, &frameSize);
        frSize = Vector2<float>{
            frameSize[0] / (float)texSize[0], frameSize[1] / (float)texSize[1] };

        tx0 = mTexpos[0] + texCoord[0];
        tx1 = mTexpos[0] + texCoord[0] + frSize[0] * mTexsize[0];
        ty0 = mTexpos[1] + texCoord[1];
        ty1 = mTexpos[1] + texCoord[1] + frSize[1] * mTexsize[1];
    }
    else 
    {
        tx0 = mTexpos[0];
        tx1 = mTexpos[0] + mTexsize[0];
        ty0 = mTexpos[1];
        ty1 = mTexpos[1] + mTexsize[1];
    }

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
        Vector4<float> color;
    };
    Vertex* vertices = mMeshBuffer->GetVertice()->Get<Vertex>();
    unsigned int vertCount = mMeshBuffer->GetVertice()->GetNumElements();

    // fill vertices
    vertices[0].position = Vector3<float>{ -mSize / 2, -mSize / 2, 0 };
    vertices[1].position = Vector3<float>{ mSize / 2, -mSize / 2, 0 };
    vertices[2].position = Vector3<float>{ mSize / 2, mSize / 2, 0 };
    vertices[3].position = Vector3<float>{ -mSize / 2, mSize / 2, 0 };

    vertices[0].tcoord = Vector2<float>{ tx1, ty1 };
    vertices[1].tcoord = Vector2<float>{ tx0, ty1 };
    vertices[2].tcoord = Vector2<float>{ tx0, ty0 };
    vertices[3].tcoord = Vector2<float>{ tx1, ty0 };

    vertices[0].color = SColorF(mColor).ToArray();
    vertices[1].color = SColorF(mColor).ToArray();
    vertices[2].color = SColorF(mColor).ToArray();
    vertices[3].color = SColorF(mColor).ToArray();

    Quaternion<float> tgt;
    Vector3<short> cameraOffset = mEnvironment->GetCameraOffset();
    for (unsigned int vertIdx = 0; vertIdx < vertCount; vertIdx++) 
    {
        if (mVertical) 
        {
            Vector3<float> ppos = mPlayer->GetPosition() / BS;

            tgt = Rotation<3, float>(AxisAngle<3, float>(-Vector3<float>::Unit(AXIS_Y),
                (std::atan2(ppos[2] - mPos[2], ppos[0] - mPos[0]) / (float)GE_C_DEG_TO_RAD + 90) * 
                (float)GE_C_DEG_TO_RAD));
            vertices[vertIdx].position = HProject(Rotate(tgt,  HLift(vertices[vertIdx].position, 0.f)));
            //rotateXZBy
        }
        else 
        {
            tgt = Rotation<3, float>(AxisAngle<3, float>(
                -Vector3<float>::Unit(AXIS_X), -mPlayer->GetPitch() * (float)GE_C_DEG_TO_RAD));
            vertices[vertIdx].position = HProject(Rotate(tgt, HLift(vertices[vertIdx].position, 0.f)));
            //rotateYZBy
            tgt = Rotation<3, float>(AxisAngle<3, float>(
                -Vector3<float>::Unit(AXIS_Y), mPlayer->GetYaw() * (float)GE_C_DEG_TO_RAD));
            vertices[vertIdx].position = HProject(Rotate(tgt, HLift(vertices[vertIdx].position, 0.f)));
            //rotateXZBy
        }
        mBBox.GrowToContain(vertices[vertIdx].position);
        vertices[vertIdx].position += mPos * BS - Vector3<float>{
            (float)cameraOffset[0], (float)cameraOffset[1], (float)cameraOffset[2]} * BS;
    }
}

/*
    ParticleSpawner
*/

ParticleSpawner::ParticleSpawner(VisualPlayer* player,
    const ParticleSpawnerParameters& parameters, uint16_t attachedId,
    std::shared_ptr<Texture2> texture, ParticleManager* particleMgr) :
    mParticleMgr(particleMgr), mParameters(parameters)
{
    mPlayer = player;
    mAttachedId = attachedId;
    mTexture = texture;
    mTime = 0;

    mSpawnTimes.reserve(parameters.amount + 1);
    for (uint16_t i = 0; i <= parameters.amount; i++) 
    {
        float spawntime = rand() / (float)RAND_MAX * parameters.time;
        mSpawnTimes.push_back(spawntime);
    }
}

void ParticleSpawner::SpawnParticle(VisualEnvironment* env,
    float radius, const Matrix4x4<float>* attachedAbsolutePosRotMatrix)
{
    Vector3<float> ppos = mPlayer->GetPosition() / BS;
    Vector3<float> pos = Vector3<float>{
        rand() / (float)RAND_MAX * (mParameters.maxPos[0] - mParameters.minPos[0]) + mParameters.minPos[0],
        rand() / (float)RAND_MAX * (mParameters.maxPos[1] - mParameters.minPos[1]) + mParameters.minPos[1],
        rand() / (float)RAND_MAX * (mParameters.maxPos[2] - mParameters.minPos[2]) + mParameters.minPos[2] };

    // Need to apply this first or the following check
    // will be wrong for attached spawners
    if (attachedAbsolutePosRotMatrix) 
    {
        pos *= BS;
        Vector4<float> position = HLift(pos, 0.f);
        attachedAbsolutePosRotMatrix->Transformation(position);
        attachedAbsolutePosRotMatrix->Translation(position);
        pos = HProject(position);
        pos /= BS;
        Vector3<short> cameraOffset = mParticleMgr->mEnvironment->GetCameraOffset();
        pos[0] += cameraOffset[0];
        pos[1] += cameraOffset[1];
        pos[2] += cameraOffset[2];
    }

    if (Length(pos - ppos) > radius)
        return;

    // Parameters for the single particle we're about to spawn
    ParticleParameters pp;
    pp.pos = pos;

    pp.vel = Vector3<float>{
        rand() / (float)RAND_MAX * (mParameters.maxVel[0] - mParameters.minVel[0]) + mParameters.minVel[0],
        rand() / (float)RAND_MAX * (mParameters.maxVel[1] - mParameters.minVel[1]) + mParameters.minVel[1],
        rand() / (float)RAND_MAX * (mParameters.maxVel[2] - mParameters.minVel[2]) + mParameters.minVel[2] };
    pp.acc = Vector3<float>{
        rand() / (float)RAND_MAX * (mParameters.maxAcc[0] - mParameters.minAcc[0]) + mParameters.minAcc[0],
        rand() / (float)RAND_MAX * (mParameters.maxAcc[1] - mParameters.minAcc[1]) + mParameters.minAcc[1],
        rand() / (float)RAND_MAX * (mParameters.maxAcc[2] - mParameters.minAcc[2]) + mParameters.minAcc[2] };

    if (attachedAbsolutePosRotMatrix) 
    {
        // Apply attachment rotation
        Vector4<float> velocity = HLift(pp.vel, 0.f);
        attachedAbsolutePosRotMatrix->Transformation(velocity);
        pp.vel = HProject(velocity);

        Vector4<float> acceleration = HLift(pp.acc, 0.f);
        attachedAbsolutePosRotMatrix->Transformation(acceleration);
        pp.acc = HProject(acceleration);
    }

    pp.expTime = rand() / (float)RAND_MAX * (mParameters.maxExpTime - mParameters.minExpTime) + mParameters.minExpTime;
    mParameters.CopyCommon(pp);

    std::shared_ptr<Texture2> texture;
    Vector2<float> texPos, texSize;
    SColor color(0xFFFFFFFF);

    if (mParameters.node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& f =
            mParticleMgr->mEnvironment->GetNodeManager()->Get(mParameters.node);
        if (!ParticleManager::GetNodeParticleParams(mParameters.node, f, pp, &texture,
            texPos, texSize, &color, mParameters.nodeTile))
        {
            return;
        }
    }
    else 
    {
        texture = mTexture;
        texPos = Vector2<float>{ 0.0f, 0.0f };
        texSize = Vector2<float>{ 1.0f, 1.0f };
    }

    // Allow keeping default random size
    if (mParameters.maxSize > 0.0f)
        pp.size = rand() / (float)RAND_MAX * (mParameters.maxSize - mParameters.minSize) + mParameters.minSize;

    std::shared_ptr<Scene> pScene = GameApplication::Get()->GetHumanView()->mScene;
    std::shared_ptr<ParticleNode> toAdd = std::make_shared< ParticleNode>(
        GameLogic::Get()->GetNewActorID(), &pScene->GetPVWUpdater(), env,
        mPlayer, pp, texture, texPos, texSize, color);
    pScene->AddSceneNode(toAdd->GetId(), toAdd);
    mParticleMgr->AddParticle(toAdd);
}

void ParticleSpawner::Step(float dTime, VisualEnvironment* env)
{
    mTime += dTime;

    static thread_local const float radius =
        (float)Settings::Get()->GetInt16("max_block_send_distance") * MAP_BLOCKSIZE;

    bool unloaded = false;
    const Transform* attachedAbsoluteTransform = nullptr;
    if (mAttachedId) 
    {
        if (GenericVisualActiveObject* attached =
            dynamic_cast<GenericVisualActiveObject *>(env->GetActiveObject(mAttachedId)))
        {
            attachedAbsoluteTransform = attached->GetAbsoluteTransform();
        }
        else unloaded = true;
    }

    if (mParameters.time != 0) 
    {
        // Spawner exists for a predefined timespan
        for (auto i = mSpawnTimes.begin(); i != mSpawnTimes.end(); ) 
        {
            if ((*i) <= mTime && mParameters.amount > 0) 
            {
                --mParameters.amount;

                // Pretend to, but don't actually spawn a particle if it is
                // attached to an unloaded object or distant from player.
                if (!unloaded)
                    SpawnParticle(env, radius, &attachedAbsoluteTransform->GetHMatrix());

                i = mSpawnTimes.erase(i);
            }
            else ++i;
        }
    }
    else 
    {
        // Spawner exists for an infinity timespan, spawn on a per-second base

        // Skip this step if attached to an unloaded object
        if (unloaded)
            return;

        for (int i = 0; i <= mParameters.amount; i++) 
        {
            if (rand() / (float)RAND_MAX < dTime)
                SpawnParticle(env, radius, &attachedAbsoluteTransform->GetHMatrix());
        }
    }
}

/*
    ParticleManager
*/

ParticleManager::ParticleManager(Scene* scene, VisualEnvironment *env) : 
    mEnvironment(env)
{
    mNextParticleSpawnerId = 0xFFFFFFFFULL + 1;
}

ParticleManager::~ParticleManager()
{
    ClearAll();
}

void ParticleManager::Step(float dTime)
{
    StepParticles(dTime);
    StepSpawners(dTime);
}

void ParticleManager::StepSpawners(float dTime)
{
    MutexAutoLock lock(mSpawnerListLock);
    for (auto i = mParticleSpawners.begin(); i != mParticleSpawners.end();) 
    {
        if (i->second->GetExpired()) 
        {
            mParticleSpawners.erase(i++);
        }
        else 
        {
            i->second->Step(dTime, mEnvironment);
            ++i;
        }
    }
}

void ParticleManager::StepParticles(float dTime)
{
    MutexAutoLock lock(mParticleListLock);
    for (auto it = mParticles.begin(); it != mParticles.end();)
    {
        if ((*it)->GetExpired())
        {
            (*it)->DetachParent();
            it = mParticles.erase(it);
        }
        else 
        {
            (*it)->Step(dTime);
            ++it;
        }
    }
}

void ParticleManager::ClearAll()
{
    MutexAutoLock lock(mSpawnerListLock);
    MutexAutoLock lock2(mParticleListLock);

    mParticleSpawners.clear();

    for (auto it = mParticles.begin(); it != mParticles.end();)
        (*it)->DetachParent();

    mParticles.clear();
}

void ParticleManager::HandleParticleEvent(VisualEvent* evt, VisualPlayer* player)
{
    switch (evt->type)
    {
        case VE_DELETE_PARTICLESPAWNER: 
        {
            DeleteParticleSpawner(evt->DeleteParticleSpawner.id);
            // no allocated memory in delete event
            break;
        }
        case VE_ADD_PARTICLESPAWNER: 
        {
            DeleteParticleSpawner(evt->AddParticleSpawner.id);

            const ParticleSpawnerParameters& param = *evt->AddParticleSpawner.parameters;
            std::shared_ptr<Texture2> texture =
                mEnvironment->GetTextureSource()->GetTextureForMesh(param.texture);

            std::shared_ptr<ParticleSpawner> toAdd = std::make_shared<ParticleSpawner>(
                player, param, evt->AddParticleSpawner.attachedId, texture, this);
            AddParticleSpawner(evt->AddParticleSpawner.id, toAdd);
            delete evt->AddParticleSpawner.parameters;
            break;
        }
        case VE_SPAWN_PARTICLE: 
        {
            ParticleParameters& param = *evt->spawnParticle;

            std::shared_ptr<Texture2> texture;
            Vector2<float> texpos, texsize;
            SColor color(0xFFFFFFFF);

            float oldsize = param.size;
            if (param.node.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& features = mEnvironment->GetNodeManager()->Get(param.node);
                if (!GetNodeParticleParams(param.node, features, param, &texture, texpos, texsize, &color, param.nodeTile))
                    texture = nullptr;
            }
            else 
            {
                texture = mEnvironment->GetTextureSource()->GetTextureForMesh(param.texture);
                texpos = Vector2<float>{ 0.0f, 0.0f };
                texsize = Vector2<float>{ 1.0f, 1.0f };
            }

            // Allow keeping default random size
            if (oldsize > 0.0f)
                param.size = oldsize;

            if (texture) 
            {
                std::shared_ptr<Scene> pScene = GameApplication::Get()->GetHumanView()->mScene;
                std::shared_ptr<ParticleNode> toAdd = std::make_shared<ParticleNode>(
                    GameLogic::Get()->GetNewActorID(), &pScene->GetPVWUpdater(), mEnvironment, 
                    player, param, texture, texpos, texsize, color);
                pScene->AddSceneNode(toAdd->GetId(), toAdd);

                AddParticle(toAdd);
            }

            delete evt->spawnParticle;
            break;
        }
        default: break;
    }
}

bool ParticleManager::GetNodeParticleParams(const MapNode& node, 
    const ContentFeatures& cFeatures, ParticleParameters& p, std::shared_ptr<Texture2>* texture, 
    Vector2<float>& texpos, Vector2<float>& texsize, SColor* color, uint8_t tilenum)
{
    // No particles for "airlike" nodes
    if (cFeatures.drawType == NDT_AIRLIKE)
        return false;

    // Texture
    uint8_t texid;
    if (tilenum > 0 && tilenum <= 6)
        texid = tilenum - 1;
    else
        texid = rand() % 6;
    const TileLayer &tile = cFeatures.tiles[texid].layers[0];
    p.animation.type = TAT_NONE;

    // Only use first frame of animated texture
    if (tile.materialFlags & MATERIAL_FLAG_ANIMATION)
        *texture = (*tile.frames)[0].texture;
    else
        *texture = tile.texture;

    float size = (rand() % 8) / 64.0f;
    p.size = BS * size;
    if (tile.scale)
        size /= tile.scale;
    texsize = Vector2<float>{ size * 2.0f, size * 2.0f };
    texpos[0] = (rand() % 64) / 64.0f - texsize[0];
    texpos[1] = (rand() % 64) / 64.0f - texsize[1];

    if (tile.hasColor)
        *color = tile.color;
    else
        node.GetColor(cFeatures, color);

    return true;
}

// The final burst of particles when a node is finally dug, *not* particles
// spawned during the digging of a node.

void ParticleManager::AddDiggingParticles(VisualPlayer* player, 
    Vector3<short> pos, const MapNode& node, const ContentFeatures& cFeatures)
{
    // No particles for "airlike" nodes
    if (cFeatures.drawType == NDT_AIRLIKE)
        return;

    for (uint16_t j = 0; j < 16; j++)
        AddNodeParticle(player, pos, node, cFeatures);
}

// During the digging of a node particles are spawned individually by this
// function, called from HandleDigging() 

void ParticleManager::AddNodeParticle(VisualPlayer* player, 
    Vector3<short> pos, const MapNode& node, const ContentFeatures& cFeatures)
{
    ParticleParameters p;
    std::shared_ptr<Texture2> texture;
    Vector2<float> texpos, texsize;
    SColor color;

    if (!GetNodeParticleParams(node, cFeatures, p, &texture, texpos, texsize, &color))
        return;

    p.expTime = (rand() % 100) / 100.0f;

    // Physics
    p.vel = Vector3<float>{
        (rand() % 150) / 50.0f - 1.5f, (rand() % 150) / 50.0f, (rand() % 150) / 50.0f - 1.5f };
    p.acc = Vector3<float>{
        0.0f, -player->mMovementGravity * player->mPhysicsOverrideGravity / BS, 0.0f};
    p.pos = Vector3<float>{
        (float)pos[0] + (rand() % 100) / 200.0f - 0.25f,
        (float)pos[1] + (rand() % 100) / 200.0f - 0.25f,
        (float)pos[2] + (rand() % 100) / 200.0f - 0.25f
    };

    std::shared_ptr<Scene> pScene = GameApplication::Get()->GetHumanView()->mScene;
    std::shared_ptr<ParticleNode> toAdd = std::make_shared<ParticleNode>(
        GameLogic::Get()->GetNewActorID(), &pScene->GetPVWUpdater(),
        mEnvironment, player, p, texture, texpos, texsize, color);
    pScene->AddSceneNode(toAdd->GetId(), toAdd);

    AddParticle(toAdd);
}

void ParticleManager::AddParticle(std::shared_ptr<ParticleNode> toAdd)
{
    MutexAutoLock lock(mParticleListLock);
    mParticles.push_back(toAdd);
}


void ParticleManager::AddParticleSpawner(uint64_t id, std::shared_ptr<ParticleSpawner> toAdd)
{
    MutexAutoLock lock(mSpawnerListLock);
    mParticleSpawners[id] = toAdd;
}

void ParticleManager::DeleteParticleSpawner(uint64_t id)
{
    MutexAutoLock lock(mSpawnerListLock);
    auto it = mParticleSpawners.find(id);
    if (it != mParticleSpawners.end()) 
        mParticleSpawners.erase(it);
}
