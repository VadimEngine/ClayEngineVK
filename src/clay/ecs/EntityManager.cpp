#include "clay/ecs/EntityManager.h"
#include "clay/graphics/common/Tilemap.h"
#include <clay/utils/common/Logger.h>

namespace clay::ecs {

// TODO template add/remove
// todo see if scene specific resources can be used

EntityManager::EntityManager(BaseGraphicsContext& gContext, Resources& resources) 
    : mGraphicsContext_(gContext), mResources_(resources), mRenderSystem_(gContext, resources) {}

EntityManager::~EntityManager() {
    // Clean up bone buffers
    releaseBoneBuffers();
    
    // Actors should be released by calling releasePhysicsActors() before destruction
    // If we still have actors here, log a warning
    for (Entity e : mCurrentEntities_) {
        if (mSignatures[e].test(ComponentType::PHYSX_RIGID_BODY)) {
            if (mPhysXRigidBodies[e].actor) {
                LOG_W("EntityManager destroyed with unreleased PhysX actors. Call releasePhysicsActors() before scene destruction.");
                break;
            }
        }
    }
}

Entity EntityManager::createEntity() {
    Entity id;

    if (!mFreeEntities.empty()) {
        id = mFreeEntities.back();
        mFreeEntities.pop_back();
    } else {
        id = static_cast<int>(mCurrentEntities_.size());
    }

    mCurrentEntities_.insert(id);
    return id;
}

void EntityManager::destroyEntity(Entity entity) {
    if (mSignatures[entity][ComponentType::PHYSX_RIGID_BODY]) {
        if (mPhysXRigidBodies[entity].actor) {
            mPhysXRigidBodies[entity].actor->release();
            mPhysXRigidBodies[entity].actor = nullptr;
        }
    }
    
    if (mSignatures[entity][ComponentType::SKELETAL_ANIMATION_RENDERABLE]) {
        auto& animRenderable = mAnimation3DRenderables[entity];
        if (animRenderable.boneTransformsBuffer_) {
            mGraphicsContext_.getDevice().destroyBuffer(animRenderable.boneTransformsBuffer_);
            animRenderable.boneTransformsBuffer_ = nullptr;
        }
        if (animRenderable.boneTransformsMemory_) {
            mGraphicsContext_.getDevice().freeMemory(animRenderable.boneTransformsMemory_);
            animRenderable.boneTransformsMemory_ = nullptr;
        }
    }

    mSignatures[entity].reset();
    mCurrentEntities_.erase(entity);
    mFreeEntities.push_back(entity);
}

template<typename T>
void EntityManager::addComponent(Entity e, const T& comp) {
    if constexpr (std::is_same_v<T, ModelRenderable>) {
        mModelRenderable[e] = comp;
        mSignatures[e].set(ComponentType::MODEL);
    } else if constexpr (std::is_same_v<T, TextRenderable>) {
        mTextRenderables[e] = comp;
        mSignatures[e].set(ComponentType::TEXT);
    } else if constexpr (std::is_same_v<T, Transform>) {
        mTransforms[e] = comp;
        mSignatures[e].set(ComponentType::TRANSFORM);
    } else if constexpr (std::is_same_v<T, Collider>) {
        mColliders[e] = comp;
        mSignatures[e].set(ComponentType::COLLIDER);
    } else if constexpr (std::is_same_v<T, PhysXRigidBody>) {
        if (mSignatures[e][ComponentType::PHYSX_RIGID_BODY]) {
            if (mPhysXRigidBodies[e].actor) {
                mPhysXRigidBodies[e].actor->release();
            }
        }
        mPhysXRigidBodies[e] = comp;
        mSignatures[e].set(ComponentType::PHYSX_RIGID_BODY);
    } else if constexpr (std::is_same_v<T, PhysicsBody2D>) {
        mPhysicsBodies2D[e] = comp;
        mSignatures[e].set(ComponentType::PHYSICS_BODY_2D);
    } else  if constexpr (std::is_same_v<T, Parent>) {
        mParents[e] = comp;
        mSignatures[e].set(ComponentType::PARENT);
    } else if constexpr (std::is_same_v<T, SpriteRenderable>) {
        mSpriteRenderables[e] = comp;
        mSignatures[e].set(ComponentType::SPRITE);
    } else if constexpr (std::is_same_v<T, Animation2DRenderable>) {
        mAnimation2DRenderables[e] = comp;
        mSignatures[e].set(ComponentType::ANIMATION_2D_RENDERABLE);
    } else if constexpr (std::is_same_v<T, Animation3DRenderable>) {
        mAnimation3DRenderables[e] = comp;
        mSignatures[e].set(ComponentType::SKELETAL_ANIMATION_RENDERABLE);
    } else if constexpr (std::is_same_v<T, EntityMetadata>) {
        mMetaData[e] = comp;
        mSignatures[e].set(ComponentType::METADATA);
    } else if constexpr (std::is_same_v<T, CollisionAction>) {
        mCollisionActions[e] = comp;
        mSignatures[e].set(ComponentType::COLLISION_ACTION);
    } else {
        static_assert(std::is_same_v<T, void>, "addComponent: unsupported component type");
    }
}

void EntityManager::releasePhysicsActors() {
    for (Entity e : mCurrentEntities_) {
        if (mSignatures[e].test(ComponentType::PHYSX_RIGID_BODY)) {
            if (mPhysXRigidBodies[e].actor) {
                mPhysXRigidBodies[e].actor->release();
                mPhysXRigidBodies[e].actor = nullptr;
            }
        }
    }
}

void EntityManager::releaseBoneBuffers() {
    for (Entity e : mCurrentEntities_) {
        if (mSignatures[e].test(ComponentType::SKELETAL_ANIMATION_RENDERABLE)) {
            auto& animRenderable = mAnimation3DRenderables[e];
            if (animRenderable.boneTransformsBuffer_) {
                mGraphicsContext_.getDevice().destroyBuffer(animRenderable.boneTransformsBuffer_);
                animRenderable.boneTransformsBuffer_ = nullptr;
            }
            if (animRenderable.boneTransformsMemory_) {
                mGraphicsContext_.getDevice().freeMemory(animRenderable.boneTransformsMemory_);
                animRenderable.boneTransformsMemory_ = nullptr;
            }
        }
    }
}

void EntityManager::render(vk::CommandBuffer cmdBuffer) {
    mRenderSystem_.render(*this, cmdBuffer);
}

void EntityManager::update(float dt) {
    mPhysicsSystem_.update(*this, dt);
    mCollisionSystem_.update(*this, dt); // TODO optimize this bottleneck
    mRenderSystem_.update(*this, dt);
}

void EntityManager::setSkybox(clay::Handle<Mesh> meshHandle, clay::Handle<Material> materialHandle) {
    mSkybox_ = std::make_unique<SkyBox>(meshHandle, materialHandle);
}

void EntityManager::addTilemap(clay::Tilemap* tilemap) {
    mTilemaps_.push_back(tilemap);
}

void EntityManager::removeTilemap(clay::Tilemap* tilemap) {
    auto it = std::find(mTilemaps_.begin(), mTilemaps_.end(), tilemap);
    if (it != mTilemaps_.end()) {
        mTilemaps_.erase(it);
    }
}

// Explicitly instantiate templates for expected types
template void EntityManager::addComponent<ModelRenderable>(Entity e, const ModelRenderable& comp);
template void EntityManager::addComponent<TextRenderable>(Entity e, const TextRenderable& comp);
template void EntityManager::addComponent<Transform>(Entity e, const Transform& comp);
template void EntityManager::addComponent<Collider>(Entity e, const Collider& comp);
template void EntityManager::addComponent<PhysXRigidBody>(Entity e, const PhysXRigidBody& comp);
template void EntityManager::addComponent<PhysicsBody2D>(Entity e, const PhysicsBody2D& comp);
template void EntityManager::addComponent<SpriteRenderable>(Entity e, const SpriteRenderable& comp);
template void EntityManager::addComponent<Animation2DRenderable>(Entity e, const Animation2DRenderable& comp);
template void EntityManager::addComponent<Animation3DRenderable>(Entity e, const Animation3DRenderable& comp);
template void EntityManager::addComponent<Parent>(Entity e, const Parent& comp);
template void EntityManager::addComponent<EntityMetadata>(Entity e, const EntityMetadata& comp);
template void EntityManager::addComponent<CollisionAction>(Entity e, const CollisionAction& comp);

} // namespace clay::ecs