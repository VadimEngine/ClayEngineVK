#pragma once
// standard lib
#include <stdint.h>
#include <bitset>
#include <array>
#include <queue>
#include <set>
// third party
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
// clay
#include "clay/application/common/Resources.h"
#include "clay/ecs/Types.h"
#include "clay/graphics/common/Model.h"
#include "clay/graphics/common/SkyBox.h"
#include "clay/ecs/components/TextRenderable.h"
#include "clay/ecs/systems/CollisionSystem.h"
#include "clay/ecs/systems/RenderSystem.h"
#include "clay/ecs/systems/PhysicsSystem.h"

namespace clay {
    class Tilemap;
}

namespace clay::ecs {

class EntityManager {
public:

    EntityManager(BaseGraphicsContext& gContext, Resources& resources);

    ~EntityManager();

    Entity createEntity();

    void destroyEntity(Entity entity);

    // Template for adding components
    template<typename T>
    void addComponent(Entity e, const T& comp);

    // Skybox management (optional, only one per scene)
    void setSkybox(clay::Handle<Mesh> meshHandle, clay::Handle<Material> materialHandle);
    bool hasSkybox() const { return mSkybox_ != nullptr; }
    SkyBox* getSkybox() const { return mSkybox_.get(); }

    // Tilemap management (can have multiple)
    void addTilemap(clay::Tilemap* tilemap);
    void removeTilemap(clay::Tilemap* tilemap);
    const std::vector<clay::Tilemap*>& getTilemaps() const { return mTilemaps_; }

    // Clean up PhysX actors before scene destruction
    void releasePhysicsActors();

    // Clean up bone buffers for skeletal animation renderables
    void releaseBoneBuffers();

    // for now, have update/render in here?
    void render(vk::CommandBuffer cmdBuffer);

    void update(float dt);

//private:
    BaseGraphicsContext& mGraphicsContext_;
    Resources& mResources_;
    CollisionSystem mCollisionSystem_;
    RenderSystem mRenderSystem_;
    PhysicsSystem mPhysicsSystem_;

    std::vector<Entity> mFreeEntities;
    std::set<Entity> mCurrentEntities_;

    std::array<Signature, MAX_ENTITIES> mSignatures{};

    std::array<Transform, MAX_ENTITIES> mTransforms;
    std::array<Parent, MAX_ENTITIES> mParents;
    std::array<ModelRenderable, MAX_ENTITIES> mModelRenderable;
    std::array<TextRenderable, MAX_ENTITIES> mTextRenderables;
    std::array<SpriteRenderable, MAX_ENTITIES> mSpriteRenderables;
    std::array<Animation2DRenderable, MAX_ENTITIES> mAnimation2DRenderables;
    std::array<Animation3DRenderable, MAX_ENTITIES> mAnimation3DRenderables;
    std::array<Collider, MAX_ENTITIES> mColliders;
    std::array<PhysXRigidBody, MAX_ENTITIES> mPhysXRigidBodies;
    std::array<PhysicsBody2D, MAX_ENTITIES> mPhysicsBodies2D;
    std::array<EntityMetadata, MAX_ENTITIES> mMetaData;
    std::array<CollisionAction, MAX_ENTITIES> mCollisionActions;

private:
    std::unique_ptr<SkyBox> mSkybox_; // Optional scene skybox
    std::vector<clay::Tilemap*> mTilemaps_; // List of tilemaps in the scene
};

} // namespace clay::ecs