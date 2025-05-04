#ifdef CLAY_PLATFORM_XR

// class
#include <clay/application/xr/CameraControllerXR.h>

namespace clay {

CameraControllerXR::CameraControllerXR(Camera* pCamera):
    mpCamera_(pCamera) {
}

void CameraControllerXR::setCamera(Camera* pCamera) {
    mpCamera_ = pCamera;
}

void CameraControllerXR::updateWithJoystickInput(const glm::vec2& leftJoystick,
                                                 const glm::vec2& rightJoystick,
                                                 float moveSpeed,
                                                 float rotationSpeed,
                                                 const XrPosef& headPose) {
    if (mpCamera_ == nullptr) {
        return;
    }

    // Threshold to consider joystick input as neutral
    const float joystickDeadZone = 0.1f;

    // Check if right joystick has movement (for orientation update)
    if (glm::length2(rightJoystick) > joystickDeadZone) {
        // Normalize the joystick input to avoid speed increase with diagonal movement
        const glm::vec2 normalizedRightJoystick = glm::normalize(rightJoystick);
        // Update orientation based on normalized right joystick input
        const float yaw = normalizedRightJoystick.x * rotationSpeed;   // Rotate left/right (yaw)
        // Yaw around Y-axis (Right hand rotation)
        glm::quat yawQuat = glm::angleAxis(-glm::radians(yaw),glm::vec3(0.0f, 1.0f, 0.0f));

        mpCamera_->getOrientation() = yawQuat * mpCamera_->getOrientation();
    }

    // Check if left joystick has movement (for position update).
    if (glm::length2(leftJoystick) > joystickDeadZone) {
        // Normalize the joystick input to avoid speed increase with diagonal movement
        const glm::vec2 normalizedLeftJoystick = glm::normalize(leftJoystick);
        const glm::quat headPoseQuat = glm::quat(headPose.orientation.w, headPose.orientation.x,headPose.orientation.y, headPose.orientation.z);
        const glm::quat combinedQuat = mpCamera_->getOrientation() * headPoseQuat;

        glm::vec3 forward = combinedQuat * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right = combinedQuat * glm::vec3(1.0f, 0.0f, 0.0f);

        // Restrict movement to the X-Z plane
        forward.y = 0.0f;
        right.y = 0.0f;

        // Normalize again to prevent unintended scaling
        forward = glm::normalize(forward);
        right = glm::normalize(right);

        const glm::vec3 movement = (forward * normalizedLeftJoystick.y + right * normalizedLeftJoystick.x) * moveSpeed;
        // Update position based on joystick input
        mpCamera_->getPosition() += movement;
    }
}

} // namespace clay

#endif