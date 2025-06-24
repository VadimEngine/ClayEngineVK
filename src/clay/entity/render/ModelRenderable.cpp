// class
#include "clay/entity/render/ModelRenderable.h"

namespace clay {

ModelRenderable::ModelRenderable(Model* pModel) : BaseRenderable(),
                                                  mModel_(pModel) {}

void ModelRenderable::render(VkCommandBuffer cmdBuffer, const glm::mat4& parentModelMat) {
    // translation matrix for position
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), mPosition_);
    //rotation matrix
    const glm::mat4 rotationMatrix = glm::mat4_cast(mOrientation_);
    // scale matrix
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), mScale_);

    glm::mat4 localModelMat = translationMat * rotationMatrix * scaleMat;

    struct PushConstants {
        glm::mat4 model;
        glm::vec4 color; // optional depending on material
    } push;

    push.model = parentModelMat * localModelMat;
    push.color = mColor_;

    mModel_->render(cmdBuffer, &push, sizeof(push));
}

const Model* ModelRenderable::getModel() const {
    return mModel_;
}

void ModelRenderable::setModel(Model* pModel) {
    mModel_ = pModel;
}

void ModelRenderable::setColor(const glm::vec4 newColor) {
    mColor_ = newColor;
}

glm::vec4 ModelRenderable::getColor() const {
    return mColor_;
}

} // namespace clay