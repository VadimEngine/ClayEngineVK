// clay
#include "clay/ecs/EntityManager.h"
#include "clay/ecs/systems/RenderSystem.h"

namespace clay::ecs {

RenderSystem::RenderSystem(BaseGraphicsContext& gContext, Resources& resources)
    : mGContext_(gContext), mResources_(resources) {}

void RenderSystem::render(EntityManager& entityManager, VkCommandBuffer cmdBuffer) {
    // TODO for (auto chunk : view<MeshHandle, Transform>()) {
    for (clay::ecs::Entity e: entityManager.mCurrentEntities_) {
        if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && entityManager.mSignatures[e][clay::ecs::ComponentType::MODEL]) {
            clay::ecs::Transform& transform = entityManager.mTransforms[e];
            clay::ecs::ModelRenderable& model = entityManager.mModelRenderable[e];

            glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), transform.mPosition_);
            const glm::mat4 rotationMatrix = glm::mat4_cast(transform.mOrientation_);
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), transform.mScale_);

            struct PushConstants {
                glm::mat4 model;
                glm::vec4 color; // optional depending on material
            } push;
            push.model = translationMat * rotationMatrix * scaleMat * model.localModelMat;
            push.color = model.mColor_;

            mResources_[model.modelHandle].render(cmdBuffer, &push, sizeof(push));
        } else if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && entityManager.mSignatures[e][clay::ecs::ComponentType::TEXT]) {
            clay::ecs::Transform& transform = entityManager.mTransforms[e];
            clay::ecs::TextRenderable& text = entityManager.mTextRenderables[e];

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
        } else if (entityManager.mSignatures[e][clay::ecs::ComponentType::TRANSFORM] && entityManager.mSignatures[e][clay::ecs::ComponentType::SPRITE]) {
            clay::ecs::Transform& transform = entityManager.mTransforms[e];
            clay::ecs::SpriteRenderable& sprite = entityManager.mSpriteRenderables[e];

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

} // namespace clay::ecs