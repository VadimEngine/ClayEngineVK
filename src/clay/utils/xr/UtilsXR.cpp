#ifdef CLAY_PLATFORM_XR


// third party
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
// class
#include "clay/utils/xr/UtilsXR.h"



XrBool32 OpenXRMessageCallbackFunction(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                       XrDebugUtilsMessageTypeFlagsEXT messageType,
                                       const XrDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                       void *pUserData) {
    // Lambda to covert an XrDebugUtilsMessageSeverityFlagsEXT to std::string. Bitwise check to concatenate multiple severities to the output string.
    auto GetMessageSeverityString = [](XrDebugUtilsMessageSeverityFlagsEXT messageSeverity) -> std::string {
        bool separator = false;

        std::string msgFlags;
        if (BitwiseCheck(messageSeverity, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)) {
            msgFlags += "VERBOSE";
            separator = true;
        }
        if (BitwiseCheck(messageSeverity, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)) {
            if (separator) {
                msgFlags += ",";
            }
            msgFlags += "INFO";
            separator = true;
        }
        if (BitwiseCheck(messageSeverity, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)) {
            if (separator) {
                msgFlags += ",";
            }
            msgFlags += "WARN";
            separator = true;
        }
        if (BitwiseCheck(messageSeverity, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
            if (separator) {
                msgFlags += ",";
            }
            msgFlags += "ERROR";
        }
        return msgFlags;
    };
    // Lambda to covert an XrDebugUtilsMessageTypeFlagsEXT to std::string. Bitwise check to concatenate multiple types to the output string.
    auto GetMessageTypeString = [](XrDebugUtilsMessageTypeFlagsEXT messageType) -> std::string {
        bool separator = false;

        std::string msgFlags;
        if (BitwiseCheck(messageType, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)) {
            msgFlags += "GEN";
            separator = true;
        }
        if (BitwiseCheck(messageType, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)) {
            if (separator) {
                msgFlags += ",";
            }
            msgFlags += "SPEC";
            separator = true;
        }
        if (BitwiseCheck(messageType, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) {
            if (separator) {
                msgFlags += ",";
            }
            msgFlags += "PERF";
        }
        return msgFlags;
    };

    // Collect message data.
    std::string functionName = (pCallbackData->functionName) ? pCallbackData->functionName : "";
    std::string messageSeverityStr = GetMessageSeverityString(messageSeverity);
    std::string messageTypeStr = GetMessageTypeString(messageType);
    std::string messageId = (pCallbackData->messageId) ? pCallbackData->messageId : "";
    std::string message = (pCallbackData->message) ? pCallbackData->message : "";

    // String stream final message.
    std::stringstream errorMessage;
    errorMessage << functionName << "(" << messageSeverityStr << " / " << messageTypeStr << "): msgNum: " << messageId << " - " << message;

    // Log and debug break.
    std::cerr << errorMessage.str() << std::endl;
    if (BitwiseCheck(messageSeverity, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        DEBUG_BREAK;
    }
    return XrBool32();
}

void DestroyOpenXRDebugUtilsMessenger(
    XrInstance m_xrInstance,
    XrDebugUtilsMessengerEXT debugUtilsMessenger) {
    // Load xrDestroyDebugUtilsMessengerEXT() function pointer as it is not default loaded by the OpenXR loader.
    PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT;
    OPENXR_CHECK(xrGetInstanceProcAddr(m_xrInstance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&xrDestroyDebugUtilsMessengerEXT), "Failed to get InstanceProcAddr.");

    // Destroy the provided XrDebugUtilsMessengerEXT.
    OPENXR_CHECK(xrDestroyDebugUtilsMessengerEXT(debugUtilsMessenger), "Failed to destroy DebugUtilsMessenger.");
}

XrDebugUtilsMessengerEXT CreateOpenXRDebugUtilsMessenger(XrInstance m_xrInstance) {
    // Fill out a XrDebugUtilsMessengerCreateInfoEXT structure specifying all severities and types.
    // Set the userCallback to OpenXRMessageCallbackFunction().
    XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugUtilsMessengerCI.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCI.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    debugUtilsMessengerCI.userCallback = (PFN_xrDebugUtilsMessengerCallbackEXT)OpenXRMessageCallbackFunction;
    debugUtilsMessengerCI.userData = nullptr;

    // Load xrCreateDebugUtilsMessengerEXT() function pointer as it is not default loaded by the OpenXR loader.
    XrDebugUtilsMessengerEXT debugUtilsMessenger{};
    PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT;
    OPENXR_CHECK(xrGetInstanceProcAddr(m_xrInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&xrCreateDebugUtilsMessengerEXT), "Failed to get InstanceProcAddr.");

    // Finally create and return the XrDebugUtilsMessengerEXT.
    OPENXR_CHECK(xrCreateDebugUtilsMessengerEXT(m_xrInstance, &debugUtilsMessengerCI, &debugUtilsMessenger), "Failed to create DebugUtilsMessenger.");
    return debugUtilsMessenger;
}

namespace clay::utils {

glm::mat4 computeHeadLockViewMatrix(const XrPosef& pose) {
    return glm::mat4_cast(glm::conjugate(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z)));
}

glm::mat4 computeWorldLockViewMatrix(const XrPosef& eyePose, const glm::vec3& cameraPosition, const glm::quat& cameraOrientation, const XrPosef& headPose) {
    const glm::quat eyeOrientation(eyePose.orientation.w, eyePose.orientation.x, eyePose.orientation.y, eyePose.orientation.z);
    const glm::vec3 eyePosition(eyePose.position.x, eyePose.position.y, eyePose.position.z);
    const glm::vec3 headPosition(headPose.position.x, headPose.position.y, headPose.position.z);

    const glm::vec3 rotatedHeadPose = cameraOrientation * headPosition;
    const glm::vec3 rotatedEyePos = cameraOrientation * (eyePosition - headPosition);

    const glm::vec3 eyePositionFinal = rotatedHeadPose + cameraPosition + rotatedEyePos;
    const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), -(eyePositionFinal));

    // Combine rotation and translation for the final view matrix
    return glm::mat4_cast(glm::conjugate(cameraOrientation * eyeOrientation)) * translationMatrix;
}

glm::mat4 computeProjectionMatrix(const XrFovf& fov, float nearZ, float farZ) {
    // Calculate the frustum bounds at the near plane
    const float left = tan(fov.angleLeft) * nearZ;
    const float right = tan(fov.angleRight) * nearZ;
    const float bottom = tan(fov.angleDown) * nearZ;
    const float top = tan(fov.angleUp) * nearZ;

    // Use glm::frustum to generate the projection matrix
    // Flip Y by swapping top/bottom TODO figure out clean fix for this maybe GLM_FORCE_DEPTH_ZERO_TO_ONE
    return glm::frustum(left, right, top, bottom, nearZ, farZ);

    // return projMat;
}

bool isRayIntersectingSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& sphereCenter, float sphereRadius) {
    const glm::vec3 oc = rayOrigin - sphereCenter;

    // Quadratic coefficients
    const float a = glm::dot(rayDir, rayDir);
    const float b = 2.0f * glm::dot(oc, rayDir);
    const float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;

    // Discriminant to check for intersection
    const float discriminant = b * b - 4.0f * a * c;
    return discriminant >= 0;
}

} // namespace clay::utils

#endif // CLAY_PLATFORM_XR
