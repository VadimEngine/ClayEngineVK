// third party
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// class
#include "clay/entity/render/BaseRenderable.h"

namespace clay {

void BaseRenderable::setEnabled(const bool isEnabled) {
    mEnabled_ = isEnabled;
}

bool BaseRenderable::isEnabled() const {
    return mEnabled_;
}

glm::vec3 BaseRenderable::getPosition() const {
    return mPosition_;
}

glm::quat BaseRenderable::getOrientation() const {
    return mOrientation_;
}

glm::quat& BaseRenderable::getOrientation() {
    return mOrientation_;
}

glm::vec3 BaseRenderable::getScale() const {
    return mScale_;
}

void BaseRenderable::setPosition(const glm::vec3& newPosition) {
    mPosition_ = newPosition;
}

void BaseRenderable::setOrientation(const glm::quat& newOrientation) {
    mOrientation_ = newOrientation;
}

void BaseRenderable::setScale(const glm::vec3& newScale) {
    mScale_ = newScale;
}

} // namespace clay