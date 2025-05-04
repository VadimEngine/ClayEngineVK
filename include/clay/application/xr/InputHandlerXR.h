#pragma once
// standard lib
#include <array>
// third party
#include <glm/glm.hpp>
#include <openxr/openxr.h>

namespace clay {

class InputHandlerXR {
public:

    enum class Button {
         Y=0, X=1, B=2, A=3, LAST = 4
    };

    enum class Hand {
        LEFT=0, RIGHT=1, LAST = 2
    };

    InputHandlerXR();

    ~InputHandlerXR();

    void initialize(XrInstance theXrInstance, XrSession theSession, XrSpace localSpace, XrSpace headSpace);

    void createActionSet();

    void suggestBindings();

    void recordCurrentBindings();

    void createActionPoses();

    void attachActionSet();

    void pollActions(XrTime predictedTime);

    const XrPosef& getAimPose(Hand hand) const;

    const XrPosef& getGripPose(Hand hand) const;

    const XrPosef& getHeadPose() const;

    const XrVector2f& getJoystickDirection(Hand hand) const;

    bool getButtonDown(Button button) const;

    float getTriggerState(Hand hand) const;

    float getGrabState(Hand hand) const;

private:
    XrInstance mXRInstance_;
    XrSession mSession_ = {};

    float m_viewHeightM = 1.5f; // todo use a constant

    XrActionSet mActionSet_;

    XrAction mGripPoseAction_;
    XrAction mAimPoseAction_;

    std::array<XrAction, 4> mButtonActions_;

    XrAction mGrabAction_;
    XrAction mTriggerAction_;
    XrAction mJoystickDirAction_;

    XrActionStateFloat mGrabState_[2] = {{XR_TYPE_ACTION_STATE_FLOAT}, {XR_TYPE_ACTION_STATE_FLOAT}};
    XrActionStateFloat mTriggerState_[2] = {{XR_TYPE_ACTION_STATE_FLOAT}, {XR_TYPE_ACTION_STATE_FLOAT}};

    std::array<XrActionStateBoolean, 4> mButtonStates_ = {{
        {XR_TYPE_ACTION_STATE_BOOLEAN},
        {XR_TYPE_ACTION_STATE_BOOLEAN},
        {XR_TYPE_ACTION_STATE_BOOLEAN},
        {XR_TYPE_ACTION_STATE_BOOLEAN}
    }};

    XrActionStateVector2f mJoystickDirState_[2] = {{XR_TYPE_ACTION_STATE_VECTOR2F}, {XR_TYPE_ACTION_STATE_VECTOR2F}};

    XrSpace mHeadSpace_;

    XrSpace mGripPoseSpace_[2];
    XrActionStatePose mGripPoseState_[2] = {{XR_TYPE_ACTION_STATE_POSE}, {XR_TYPE_ACTION_STATE_POSE}};
    XrSpace mAimPoseSpace_[2];
    XrActionStatePose mAimPoseState_[2] = {{XR_TYPE_ACTION_STATE_POSE}, {XR_TYPE_ACTION_STATE_POSE}};

    XrSpace mLocalSpace_ = XR_NULL_HANDLE;

    XrPosef mHeadPose_ = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}};

    XrPosef mGripPose_[2] = {
            {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}},
            {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}}};

    XrPosef mAimPose_[2] = {
            {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}},
            {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}}};

    XrPath mHandPaths_[2] = {0, 0};
};

} // namespace clay