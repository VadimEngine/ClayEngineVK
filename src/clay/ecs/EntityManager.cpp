#include "clay/ecs/EntityManager.h"

namespace clay::ecs {


EntityManager::EntityManager(Resources& resources) 
    : mResources_(resources){
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
        mAvailableEntities.push(entity);
    }
}

EntityManager::~EntityManager() {}

Entity EntityManager::createEntity() {
    Entity id = mAvailableEntities.front();
    mAvailableEntities.pop();

    mCurrentEntities_.insert(id);
    return id;
}

void EntityManager::destroyEntity(Entity entity) {
    mSignatures[entity].reset();
    mAvailableEntities.push(entity);

    mCurrentEntities_.erase(entity);
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

void EntityManager::render(VkCommandBuffer cmdBuffer) {
    for (clay::ecs::Entity e: mCurrentEntities_) {
        if (mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && mSignatures[e][clay::ecs::ComponentType::MODEL]) {
            clay::ecs::Transform& transform = mTransforms[e];
            clay::ecs::ModelRenderable& model = mModelRenderable[e];

            glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
            const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

            struct PushConstants {
                glm::mat4 model;
                glm::vec4 color; // optional depending on material
            } push;
            push.model = translationMat * rotationMatrix * scaleMat * model.localModelMat;
            push.color = model.mColor_;

            mResources_.mModelsPool_[model.modelHandle].render(cmdBuffer, &push, sizeof(push));

            // model.mModel_->render(cmdBuffer, &push, sizeof(push));
        } else if (mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && mSignatures[e][clay::ecs::ComponentType::TEXT]) {
            clay::ecs::Transform& transform = mTransforms[e];
            clay::ecs::TextRenderable& text = mTextRenderables[e];

            glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
            const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

            text.mpFont_->getMaterial().bindMaterial(cmdBuffer);

            struct PushConstants {
                glm::mat4 model;
                glm::vec4 color;
            } push;

            push.color = text.mColor_;
            push.model = translationMat * rotationMatrix * scaleMat * glm::scale(glm::mat4(1.0f), text.mScale_);;

            text.mpFont_->getMaterial().pushConstants(cmdBuffer, &push, sizeof(PushConstants),  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

            VkBuffer vertexBuffers[] = { text.mVertexBuffer_ };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(cmdBuffer, static_cast<uint32_t>(text.mVertices_.size()), 1, 0, 0);
        } else if (mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && mSignatures[e][clay::ecs::ComponentType::SPRITE]) {
            clay::ecs::Transform& transform = mTransforms[e];
            clay::ecs::SpriteRenderable& sprite = mSpriteRenderables[e];

            sprite.mpMaterial_->bindMaterial(cmdBuffer);

            glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
            const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

            struct PushConstants {
                glm::mat4 model;
                glm::vec4 color;
                glm::vec4 offsets;
            } push;

            push.model = translationMat * rotationMatrix * scaleMat;
            push.color = sprite.mColor_;
            push.offsets = sprite.mSpriteOffset_;

            sprite.mpMaterial_->pushConstants(
                cmdBuffer,
                &push,
                sizeof(push),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            );

            sprite.mpMesh_->bindMesh(cmdBuffer);
            vkCmdDrawIndexed(cmdBuffer, sprite.mpMesh_->getIndicesCount(), 1, 0, 0, 0);
        }
    }
}

void update(float dt);

} // namespace clay::ecs