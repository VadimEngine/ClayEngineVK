// class
#include "clay/graphics/common/Camera.h"

namespace clay {

Camera::Camera(const glm::vec3& position) {
    mPosition_ = position;
    mForward_ = glm::vec3(0.0f, 0.0f, -1.0f);
    mUp_ = glm::vec3(0.0f, 1.0f, 0.0f);
    mRight_ = glm::normalize(glm::cross(mForward_, glm::vec3(0.0f, 1.0f, 0.0f)));
    mOrientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
    updateCameraVectors();
}

Camera::~Camera() {}

void Camera::update(float dt) {}

void Camera::move(const glm::vec3& dir, const float step) {
    mPosition_ += (glm::normalize(dir) * step);
}

void Camera::rotate(const glm::vec3& axis, const float angle) {
    glm::quat rotation = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
    mOrientation_ = glm::normalize(rotation * mOrientation_); // Apply rotation
    updateCameraVectors();
}

void Camera::setFOV(const float newFOV) {
    mFOV_ = newFOV;
}

void Camera::setAspectRatio(float newAspect) {
    mAspectRatio_ = newAspect;
}

void Camera::setPosition(const glm::vec3& newPosition) {
    mPosition_ = newPosition;
}

void Camera::setOrientation(const glm::quat& newOrientation) {
    mOrientation_ = glm::normalize(newOrientation);
    updateCameraVectors();
}

void Camera::zoom(const float zoomAdjust) {
    mFOV_ += zoomAdjust;
    if (mFOV_ < 0) {
        mFOV_ = 0;
    }
}

void Camera::setMode(const CameraMode mode) {
    mMode_ = mode;
}

glm::mat4 Camera::getProjectionMatrix() const {
    if (mMode_ == CameraMode::PERSPECTIVE) {
        auto projection = glm::perspective(glm::radians(mFOV_), mAspectRatio_, 0.1f, 100.0f);
        projection[1][1] *= -1; // needed for vulkan since y points down
        return projection;
    } else if (mMode_ == CameraMode::ORTHOGONAL) {
        return glm::ortho(-2.0f, +2.0f, -1.5f, +1.5f, 0.1f, 100.0f);
    }
    return glm::mat4(1);
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(mPosition_, mPosition_ + mForward_, mUp_);
}

glm::quat Camera::getOrientation() const {
    return mOrientation_;
}

glm::quat& Camera::getOrientation() {
    return mOrientation_;
}

glm::vec3 Camera::getPosition() const {
    return mPosition_;
}

glm::vec3& Camera::getPosition() {
    return mPosition_;
}

glm::vec3 Camera::getForward() const {
    return mForward_;
}

glm::vec3 Camera::getRight() const {
    return mRight_;
}

glm::vec3 Camera::getUp() const {
    return mUp_;
}

float Camera::getFOV() const {
    return mFOV_;
}

float Camera::getAspectRatio() const {
    return mAspectRatio_;
}

Camera::CameraMode Camera::getMode() const {
    return mMode_;
}

void Camera::setMoveSpeed(float newSpeed) {
    mMoveSpeed_ = newSpeed;
}

void Camera::setRotationSpeed(float newSpeed) {
    mRotationSpeed_ = newSpeed;
}

void Camera::setZoomSpeed(float newSpeed) {
    mZoomSpeed_ = newSpeed;
}

float Camera::getMoveSpeed() const {
    return mMoveSpeed_;
}

float Camera::getRotationSpeed() const {
    return mRotationSpeed_;
}

float Camera::getZoomSpeed() const {
    return mZoomSpeed_;
}

void Camera::updateCameraVectors() {
    // Convert quaternion orientation to a rotation matrix
    glm::mat4 rotationMatrix = glm::mat4_cast(mOrientation_);

    // Apply rotation to the default forward vector
    mForward_ = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    mRight_ = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
    mUp_ = glm::normalize(glm::cross(mRight_, mForward_));
}

float Camera::getNear() const {
    return mNear_;
}

float Camera::getFar() const {
    return mFar_;
}

} // namespace clay