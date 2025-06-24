#include "clay/entity/render/SpriteRenderable.h"

namespace clay {

SpriteRenderable::SpriteRenderable(Mesh* pMesh, Material* pMaterial, glm::vec4 spriteOffset) 
    : mpMesh_(pMesh),
     mpMaterial_(pMaterial),
     mSpriteOffset_(spriteOffset),
     mColor_({1.0f, 1.0f, 1.0f, 1.0f}) {}

SpriteRenderable::~SpriteRenderable() {}

void SpriteRenderable::render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) {
    mpMaterial_->bindMaterial(cmdBuffer);



    // translation matrix
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), mPosition_);
    //rotation matrix
    const glm::mat4 rotationMatrix = glm::mat4_cast(mOrientation_);
    // scale matrix
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), mScale_);

    glm::mat4 localModelMat = translationMat * rotationMatrix * scaleMat;

    struct PushConstants {
        glm::mat4 model;
        glm::vec4 color;
        glm::vec4 offsets;
    } push;

    push.model = parentModelMat * localModelMat;
    push.color = mColor_;
    push.offsets = mSpriteOffset_;

    mpMaterial_->pushConstants(
        cmdBuffer,
        &push,
        sizeof(push),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    mpMesh_->bindMesh(cmdBuffer);
    vkCmdDrawIndexed(cmdBuffer, mpMesh_->getIndicesCount(), 1, 0, 0, 0);
}

void SpriteRenderable::setColor(const glm::vec4 newColor) {

}

} // namespace clay
