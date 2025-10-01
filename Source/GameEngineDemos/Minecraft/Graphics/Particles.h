/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef PARTICLES_H
#define PARTICLES_H

#include "GameEngineStd.h"

#include "Actors/VisualPlayer.h"

#include "../Data/ParticleParams.h"

#include "../Games/Map/MapNode.h"

#include "Graphic/Scene/Scene.h"
#include "Graphic/Scene/Hierarchy/Node.h"
#include "Graphic/Scene/Hierarchy/BoundingBox.h"

class PVWUpdater;
class ParticleManager;
class VisualEnvironment;

struct VisualEvent;
struct ContentFeatures;


// This file defines the particle-related structures that both the logic and
// visual need. The ParticleManager and rendering is in visual/particles.h
class ParticleNode : public Node
{
public:
    ParticleNode(const ActorId actorId, 
        PVWUpdater* updater, VisualEnvironment* env, VisualPlayer* player, 
        const ParticleParameters& parameters, std::shared_ptr<Texture2> texture,
        Vector2<float> texpos, Vector2<float> texsize, SColor color);

    //destructor
    virtual ~ParticleNode();

    virtual BoundingBox<float>& GetBoundingBox()
    {
        return mBBox;
    }

    virtual size_t GetMaterialCount() const
    {
        return 1;
    }

    virtual std::shared_ptr<Material> const& GetMaterial(unsigned int i)
    {
        return mMeshBuffer->GetMaterial();
    }

    virtual bool PreRender(Scene* pScene);
    virtual bool Render(Scene* pScene);

    void Step(float dTime);

    bool GetExpired()
    {
        return mExpiration < mTime;
    }

private:
    void UpdateLight();
    void UpdateVertices();

    std::shared_ptr<BlendState> mBlendState;
    std::shared_ptr<DepthStencilState> mDepthStencilState;
    std::shared_ptr<RasterizerState> mRasterizerState;

    std::shared_ptr<Visual> mVisual;
    std::shared_ptr<MeshBuffer> mMeshBuffer;
    std::shared_ptr<VisualEffect> mEffect;

    float mTime = 0.0f;
    float mExpiration;

    VisualEnvironment* mEnvironment;
    BoundingBox<float> mBBox;
    BoundingBox<float> mCollisionBox;
    Vector2<float> mTexpos;
    Vector2<float> mTexsize;
    Vector3<float> mPos;
    Vector3<float> mVelocity;
    Vector3<float> mAcceleration;
    VisualPlayer* mPlayer;
    float mSize;
    //! Color without lighting
    SColor mBaseColor;
    //! Final rendered color
    SColor mColor;
    bool mCollisionDetection;
    bool mCollisionRemoval;
    bool mObjectCollision;
    bool mVertical;
    Vector3<short> mCameraOffset;
    struct TileAnimationParams mAnimation;
    float mAnimationTime = 0.0f;
    int mAnimationFrame = 0;
    uint8_t mGlow;
};

class ParticleSpawner
{
public:
    ParticleSpawner(VisualPlayer* player,
        const ParticleSpawnerParameters& p, uint16_t attachedId,
        std::shared_ptr<Texture2> texture, ParticleManager* particleMgr);

    ~ParticleSpawner() = default;

    void Step(float dTime, VisualEnvironment* env);

    bool GetExpired()
    {
        return mParameters.amount <= 0 && mParameters.time != 0;
    }

private:
    void SpawnParticle(VisualEnvironment* env, 
        float radius, const Matrix4x4<float>* attachedAbsolutePosRotMatrix);

    float mTime;
    VisualPlayer* mPlayer;
    ParticleManager* mParticleMgr;
    ParticleSpawnerParameters mParameters;
    std::shared_ptr<Texture2> mTexture;
    std::vector<float> mSpawnTimes;
    uint16_t mAttachedId;
};

/**
 * Class doing particle as well as their spawners handling
 */
class ParticleManager
{
    friend class ParticleSpawner;
public:
    ParticleManager(Scene* scene, VisualEnvironment* env);
    ~ParticleManager();

    void Step(float dTime);

    void HandleParticleEvent(VisualEvent* evt, VisualPlayer* player);

    void AddDiggingParticles(
        VisualPlayer* player, Vector3<short> pos,
        const MapNode& node, const ContentFeatures& f);

    void AddNodeParticle(
        VisualPlayer* player, Vector3<short> pos,
        const MapNode& node, const ContentFeatures& f);

    /**
     * This function is only used by visual particle spawners
     *
     * We don't need to check the particle spawner list because visual ID will
     * never overlap (uint64_t)
     * @return new id
     */
    uint64_t GenerateSpawnerId()
    {
        return mNextParticleSpawnerId++;
    }

protected:
    static bool GetNodeParticleParams(const MapNode& node, const ContentFeatures& f,
        ParticleParameters& p, std::shared_ptr<Texture2>* texture, Vector2<float>& texpos,
        Vector2<float>& texsize, SColor* color, uint8_t tilenum = 0);

    void AddParticle(std::shared_ptr<ParticleNode> tAadd);

private:
    void AddParticleSpawner(uint64_t id, std::shared_ptr<ParticleSpawner> spawner);
    void DeleteParticleSpawner(uint64_t id);

    void StepParticles(float dTime);
    void StepSpawners(float dTime);

    void ClearAll();

    std::vector<std::shared_ptr<ParticleNode>> mParticles;
    std::unordered_map<uint64_t, std::shared_ptr<ParticleSpawner>> mParticleSpawners;
    // Start the particle spawner ids generated from here after unsigned int_max. lower values are
    // for logic sent spawners.
    uint64_t mNextParticleSpawnerId;

    VisualEnvironment* mEnvironment;
    std::mutex mParticleListLock;
    std::mutex mSpawnerListLock;
};

#endif