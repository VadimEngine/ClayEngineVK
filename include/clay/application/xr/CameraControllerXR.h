#pragma once
#ifdef CLAY_PLATFORM_XR

// third party
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <openxr/openxr.h>
// project
#include "clay/graphics/common/Camera.h"

namespace clay {

class CameraControllerXR {
public:

    CameraControllerXR(Camera* pCamera);

    void setCamera(Camera* pCamera);

    void updateWithJoystickInput(const glm::vec2 &leftJoystick,
                                 const glm::vec2 &rightJoystick,
                                 float moveSpeed,
                                 float rotationSpeed,
                                 const XrPosef &headOrientation);

private:
    Camera* mpCamera_ = nullptr;
};

} // namespace clay

#endif