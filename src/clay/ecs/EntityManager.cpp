#include "clay/ecs/EntityManager.h"

namespace clay::ecs {

// TODO template add/remove
// todo see if scene specific resources can be used

EntityManager::EntityManager(BaseGraphicsContext& gContext, Resources& resources) 
    : mResources_(resources), mRenderSystem_(gContext, resources) {}

EntityManager::~EntityManager() {}

Entity EntityManager::createEntity() {
    Entity id;

    if (!mFreeEntities.empty()) {
        id = mFreeEntities.back();
        mFreeEntities.pop_back();
    } else {
        // TODO static cast
        id = mCurrentEntities_.size();
    }

    mCurrentEntities_.insert(id);
    return id;
}

void EntityManager::destroyEntity(Entity entity) {
    mSignatures[entity].reset();
    mCurrentEntities_.erase(entity);
    mFreeEntities.push_back(entity);  
}

void EntityManager::addModelRenderable(Entity e, const ModelRenderable& comp) {
    mModelRenderable[e] = comp;
    mSignatures[e].set(ComponentType::MODEL);
}

// TODO fix this. Right now it requires TextRenderable to be initialized
void EntityManager::addTextRenderable(Entity e, const TextRenderable& comp) {
    mTextRenderables[e] = comp;
    mSignatures[e].set(ComponentType::TEXT);
}

void EntityManager::addTransform(Entity e, const Transform& comp) {
    mTransforms[e] = comp;
    mSignatures[e].set(ComponentType::TRANSFORM);
}

void EntityManager::addCollider(Entity e, const Collider& comp) {
    mColliders[e] = comp;
    mSignatures[e].set(ComponentType::COLLIDER);
}

void EntityManager::addRigidBody(Entity e, const RigidBody& comp) {
    mRigidBodies[e] = comp;
    mSignatures[e].set(ComponentType::RIGID_BODY);
}

void EntityManager::addSpriteRenderable(Entity e, const SpriteRenderable& comp) {
    mSpriteRenderables[e] = comp;
    mSignatures[e].set(ComponentType::SPRITE);
}

void EntityManager::render(vk::CommandBuffer cmdBuffer) {
    mRenderSystem_.render(*this, cmdBuffer);
}

void update(float dt);

} // namespace clay::ecs