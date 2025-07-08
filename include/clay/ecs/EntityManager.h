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
#include "clay/ecs/components/TextRenderable.h"
#include "clay/ecs/systems/RenderSystem.h"

namespace clay::ecs {

class EntityManager {
public:

    EntityManager(BaseGraphicsContext& gContext, Resources& resources);

    ~EntityManager();

    Entity createEntity();

    void destroyEntity(Entity entity);

    void addModelRenderable(Entity e, const ModelRenderable& comp);

    void addTextRenderable(Entity e, const TextRenderable& comp);

    void addTransform(Entity e, const Transform& comp);

    void addCollider(Entity e, const Collider& comp);

    void addRigidBody(Entity e, const RigidBody& comp);

    void addSpriteRenderable(Entity e, const SpriteRenderable& comp);

    // for now, have update/render in here?
    void render(VkCommandBuffer cmdBuffer);

    void update(float dt);

//private:
    Resources& mResources_;
    RenderSystem mRenderSystem_;

    std::vector<Entity> mFreeEntities;
    std::set<Entity> mCurrentEntities_;

	std::array<Signature, MAX_ENTITIES> mSignatures{};

    std::array<Transform, MAX_ENTITIES> mTransforms;
    std::array<ModelRenderable, MAX_ENTITIES> mModelRenderable;
    std::array<TextRenderable, MAX_ENTITIES> mTextRenderables;
    std::array<SpriteRenderable, MAX_ENTITIES> mSpriteRenderables; 
    std::array<Collider, MAX_ENTITIES> mColliders;
    std::array<RigidBody, MAX_ENTITIES> mRigidBodies;
};

} // namespace clay::ecs