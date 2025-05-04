// third party
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
// class
#include "clay/entity/Entity.h"

namespace clay {


Entity::Entity(): mPosition_(0,0,0),
                  mOrientation_(1.0f, 0.0f, 0.0f, 0.0f),
                  mScale_(1,1,1) {}

Entity::~Entity() {}

void Entity::render(VkCommandBuffer cmdBuffer) const {
    // translation matrix for position
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), mPosition_);
    // rotation matrix
    const glm::mat4 rotationMatrix = glm::mat4_cast(mOrientation_);
    // scale matrix
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), mScale_);

    const glm::mat4 modelMat = translationMatrix * rotationMatrix * scaleMatrix;

    for (auto mRenderable : mRenderables_) {
        mRenderable->render(cmdBuffer, modelMat);
    }
}

void Entity::addRenderable(BaseRenderable* newRenderable) {
    mRenderables_.push_back(newRenderable);
}

void Entity::setEnabled(const bool isEnabled) {
    mEnabled_ = isEnabled;
}

bool Entity::isEnabled() const {
    return mEnabled_;
}

glm::vec3 Entity::getPosition() const {
    return mPosition_;
}

glm::quat Entity::getOrientation() const {
    return mOrientation_;
}

glm::quat& Entity::getOrientation() {
    return mOrientation_;
}

glm::vec3 Entity::getScale() const {
    return mScale_;
}

void Entity::setPosition(const glm::vec3& newPosition) {
    mPosition_ = newPosition;
}

void Entity::setOrientation(const glm::quat& newOrientation) {
    mOrientation_ = newOrientation;
}

void Entity::setScale(const glm::vec3& newScale) {
    mScale_ = newScale;
}

} // namespace clay