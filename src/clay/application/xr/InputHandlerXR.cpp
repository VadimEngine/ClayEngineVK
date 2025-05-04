#ifdef CLAY_PLATFORM_XR

// standard lib
#include <vector>
// project
#include "clay/utils/common/Logger.h"
// class
#include "clay/application/xr/InputHandlerXR.h"

// TODO move this
#define OPENXR_CHECK(x, y)                                                                          \
{                                                                                                   \
    XrResult result = (x);                                                                          \
    if (!XR_SUCCEEDED(result)) {                                                                    \
        LOG_E("OPENXR Error: %d", int(result));                                                     \
    }                                                                                               \
}

namespace clay {

XrPath createXrPath(const char* pathString, XrInstance theXrInstance) {
    XrPath xrPath;
    OPENXR_CHECK(xrStringToPath(theXrInstance, pathString, &xrPath), "Failed to create XrPath from string.")
    return xrPath;
}

InputHandlerXR::InputHandlerXR() = default;

InputHandlerXR::~InputHandlerXR() = default;

void InputHandlerXR::initialize(XrInstance theXrInstance, XrSession theSession, XrSpace localSpace, XrSpace headSpace) {
    mXRInstance_ = theXrInstance;
    mSession_ = theSession;
    mLocalSpace_ = localSpace;
    mHeadSpace_= headSpace;

   createActionSet();
   suggestBindings();
   createActionPoses();
   attachActionSet();
}

void InputHandlerXR::createActionSet() {
    XrActionSetCreateInfo actionSetCI{XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy(actionSetCI.actionSetName, "actionset", XR_MAX_ACTION_SET_NAME_SIZE);
    strncpy(actionSetCI.localizedActionSetName, "ActionSet", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    actionSetCI.priority = 0;

    OPENXR_CHECK(xrCreateActionSet(mXRInstance_, &actionSetCI, &mActionSet_), "Failed to create ActionSet.")

    auto createAction = [this](XrInstance theXrInstance, XrAction &xrAction, const char* name, XrActionType xrActionType, const std::vector<const char*>& subaction_paths = {}) -> void {
        XrActionCreateInfo actionCI{XR_TYPE_ACTION_CREATE_INFO};
        // The type of action: float input, pose, haptic output etc.
        actionCI.actionType = xrActionType;
        // Subaction paths, e.g. left and right hand. To distinguish the same action performed on different devices.
        std::vector<XrPath> subaction_xrpaths;
        for (auto p : subaction_paths) {
            subaction_xrpaths.push_back(createXrPath(p, theXrInstance));
        }
        actionCI.countSubactionPaths = (uint32_t)subaction_xrpaths.size();
        actionCI.subactionPaths = subaction_xrpaths.data();
        // The internal name the runtime uses for this Action.
        strncpy(actionCI.actionName, name, XR_MAX_ACTION_NAME_SIZE);
        // Localized names are required so there is a human-readable action name to show the user if they are rebinding the Action in an options screen.
        strncpy(actionCI.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
        OPENXR_CHECK(xrCreateAction(mActionSet_, &actionCI, &xrAction), "Failed to create Action.")
    };

    createAction(
        mXRInstance_,
        mButtonActions_[static_cast<int>(InputHandlerXR::Button::Y)],
        "y-button",
        XR_ACTION_TYPE_BOOLEAN_INPUT
    );
    createAction(
        mXRInstance_,
        mButtonActions_[static_cast<int>(InputHandlerXR::Button::X)],
        "x-button",
        XR_ACTION_TYPE_BOOLEAN_INPUT
    );
    createAction(
        mXRInstance_,
        mButtonActions_[static_cast<int>(InputHandlerXR::Button::B)],
        "b-button",
        XR_ACTION_TYPE_BOOLEAN_INPUT
    );
    createAction(
        mXRInstance_,
        mButtonActions_[static_cast<int>(InputHandlerXR::Button::A)],
        "a-button",
        XR_ACTION_TYPE_BOOLEAN_INPUT
    );

    createAction(mXRInstance_, mGrabAction_, "grab", XR_ACTION_TYPE_FLOAT_INPUT, {"/user/hand/left", "/user/hand/right"});
    createAction(mXRInstance_, mTriggerAction_, "trigger", XR_ACTION_TYPE_FLOAT_INPUT, {"/user/hand/left", "/user/hand/right"});
    createAction(mXRInstance_, mGripPoseAction_, "palm-pose", XR_ACTION_TYPE_POSE_INPUT, {"/user/hand/left", "/user/hand/right"});
    createAction(mXRInstance_, mAimPoseAction_, "aim-pose", XR_ACTION_TYPE_POSE_INPUT, {"/user/hand/left", "/user/hand/right"});

    createAction(mXRInstance_, mJoystickDirAction_, "joystick-dir", XR_ACTION_TYPE_VECTOR2F_INPUT, {"/user/hand/left", "/user/hand/right"});

    // For later convenience we create the XrPaths for the subaction path names.
    mHandPaths_[0] = createXrPath("/user/hand/left", mXRInstance_);
    mHandPaths_[1] = createXrPath("/user/hand/right", mXRInstance_);
}

void InputHandlerXR::suggestBindings() {
    auto suggestBindings = [this](XrInstance theXrInstance, const char* profile_path, std::vector<XrActionSuggestedBinding> bindings) -> bool {
        // The application can call xrSuggestInteractionProfileBindings once per interaction profile that it supports.
        XrInteractionProfileSuggestedBinding interactionProfileSuggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        interactionProfileSuggestedBinding.interactionProfile = createXrPath(profile_path, theXrInstance);
        interactionProfileSuggestedBinding.suggestedBindings = bindings.data();
        interactionProfileSuggestedBinding.countSuggestedBindings = (uint32_t)bindings.size();
        if (xrSuggestInteractionProfileBindings(theXrInstance, &interactionProfileSuggestedBinding) == XrResult::XR_SUCCESS)
            return true;
        //XR_TUT_LOG("Failed to suggest bindings with " << profile_path);
        return false;
    };

    bool any_ok = false;

    any_ok |= suggestBindings(
            mXRInstance_,
            "/interaction_profiles/oculus/touch_controller",
            {
                    {mGrabAction_, createXrPath("/user/hand/left/input/squeeze/value", mXRInstance_)},
                    {mGrabAction_, createXrPath("/user/hand/right/input/squeeze/value", mXRInstance_)},
                    {mButtonActions_[static_cast<int>(InputHandlerXR::Button::Y)], createXrPath("/user/hand/left/input/y/click", mXRInstance_)},
                    {mButtonActions_[static_cast<int>(InputHandlerXR::Button::X)], createXrPath("/user/hand/left/input/x/click", mXRInstance_)},
                    {mButtonActions_[static_cast<int>(InputHandlerXR::Button::B)], createXrPath("/user/hand/right/input/b/click", mXRInstance_)},
                    {mButtonActions_[static_cast<int>(InputHandlerXR::Button::A)], createXrPath("/user/hand/right/input/a/click", mXRInstance_)},
                    {mTriggerAction_, createXrPath("/user/hand/left/input/trigger/value", mXRInstance_)},
                    {mTriggerAction_, createXrPath("/user/hand/right/input/trigger/value", mXRInstance_)},
                    {mGripPoseAction_, createXrPath("/user/hand/left/input/grip/pose", mXRInstance_)},
                    {mGripPoseAction_, createXrPath("/user/hand/right/input/grip/pose", mXRInstance_)},
                    {mAimPoseAction_, createXrPath("/user/hand/left/input/aim/pose", mXRInstance_)},
                    {mAimPoseAction_, createXrPath("/user/hand/right/input/aim/pose", mXRInstance_)},
                    {mJoystickDirAction_, createXrPath("/user/hand/left/input/thumbstick", mXRInstance_)},
                    {mJoystickDirAction_, createXrPath("/user/hand/right/input/thumbstick", mXRInstance_)},
                // TOOD /input/thumbstick/click,
            }
    );
    if (!any_ok) {
        LOG_E("Suggested binding failed");
    }

}

void InputHandlerXR::recordCurrentBindings() {
    if (mSession_) {
        // now we are ready to:
        XrInteractionProfileState interactionProfile = {XR_TYPE_INTERACTION_PROFILE_STATE, nullptr, 0};
        // for each action, what is the binding?
        OPENXR_CHECK(xrGetCurrentInteractionProfile(mSession_, mHandPaths_[0], &interactionProfile), "Failed to get profile.")
        if (interactionProfile.interactionProfile) {
            //XR_TUT_LOG("user/hand/left ActiveProfile " << FromXrPath(interactionProfile.interactionProfile).c_str());
        }
        OPENXR_CHECK(xrGetCurrentInteractionProfile(mSession_, mHandPaths_[1], &interactionProfile), "Failed to get profile.")
        if (interactionProfile.interactionProfile) {
            //XR_TUT_LOG("user/hand/right ActiveProfile " << FromXrPath(interactionProfile.interactionProfile).c_str());
        }
    }
}

void InputHandlerXR::createActionPoses() {
    // Create an xrSpace for a pose action.
    auto CreateActionPoseSpace = [this](XrSession session, XrAction xrAction, const char* subaction_path = nullptr) -> XrSpace {
        XrSpace xrSpace;
        const XrPosef xrPoseIdentity = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
        // Create frame of reference for a pose action
        XrActionSpaceCreateInfo actionSpaceCI{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceCI.action = xrAction;
        actionSpaceCI.poseInActionSpace = xrPoseIdentity;
        if (subaction_path)
            actionSpaceCI.subactionPath = createXrPath(subaction_path, mXRInstance_);
        OPENXR_CHECK(xrCreateActionSpace(session, &actionSpaceCI, &xrSpace), "Failed to create ActionSpace.")
        return xrSpace;
    };

    mGripPoseSpace_[0] = CreateActionPoseSpace(mSession_, mGripPoseAction_, "/user/hand/left");
    mGripPoseSpace_[1] = CreateActionPoseSpace(mSession_, mGripPoseAction_, "/user/hand/right");

    mAimPoseSpace_[0] = CreateActionPoseSpace(mSession_, mAimPoseAction_, "/user/hand/left");
    mAimPoseSpace_[1] = CreateActionPoseSpace(mSession_, mAimPoseAction_, "/user/hand/right");
}

void InputHandlerXR::attachActionSet() {
    // Attach the action set we just made to the session. We could attach multiple action sets!
    XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    actionSetAttachInfo.countActionSets = 1;
    actionSetAttachInfo.actionSets = &mActionSet_;
    OPENXR_CHECK(xrAttachSessionActionSets(mSession_, &actionSetAttachInfo), "Failed to attach ActionSet to Session.")
}

void InputHandlerXR::pollActions(XrTime predictedTime) {
    // Update our action set with up-to-date input data.
    // First, we specify the actionSet we are polling.
    XrActiveActionSet activeActionSet{};
    activeActionSet.actionSet = mActionSet_;
    activeActionSet.subactionPath = XR_NULL_PATH;
    // Now we sync the Actions to make sure they have current data.
    XrActionsSyncInfo actionsSyncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    actionsSyncInfo.countActiveActionSets = 1;
    actionsSyncInfo.activeActionSets = &activeActionSet;
    OPENXR_CHECK(xrSyncActions(mSession_, &actionsSyncInfo), "Failed to sync Actions.")
    XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};

    // get headpose
    {
        XrSpaceLocation headLocation = {XR_TYPE_SPACE_LOCATION};
        XrResult result = xrLocateSpace(
            mHeadSpace_,         // Reference to the user's head space
            mLocalSpace_,     // The space you're measuring against (e.g., XR_REFERENCE_SPACE_TYPE_STAGE)
            predictedTime,        // Time for the predicted pose
            &headLocation       // The location output
        );
        if (XR_UNQUALIFIED_SUCCESS(result) &&
            (headLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            (headLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
            mHeadPose_ = headLocation.pose;
        } else {
            LOG_E("Failed to get headpose %d", result);
        }
    }
    // We pose a single Action, twice - once for each subAction Path.
    actionStateGetInfo.action = mGripPoseAction_;
    // For each hand, get the pose state if possible.
    for (int i = 0; i < 2; i++) {
        // Specify the subAction Path.
        actionStateGetInfo.subactionPath = mHandPaths_[i];
        OPENXR_CHECK(xrGetActionStatePose(mSession_, &actionStateGetInfo, &mGripPoseState_[i]), "Failed to get Pose State.")
        if (mGripPoseState_[i].isActive) {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            XrResult res = xrLocateSpace(mGripPoseSpace_[i], mLocalSpace_, predictedTime, &spaceLocation);
            if (XR_UNQUALIFIED_SUCCESS(res) &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                mGripPose_[i] = spaceLocation.pose;
            } else {
                mGripPoseState_[i].isActive = false;
            }
        }
    }

    // Get Aim pose
    actionStateGetInfo.action = mAimPoseAction_;
    // For each hand, get the pose state if possible.
    for (int i = 0; i < 2; i++) {
        // Specify the subAction Path.
        actionStateGetInfo.subactionPath = mHandPaths_[i];
        OPENXR_CHECK(xrGetActionStatePose(mSession_, &actionStateGetInfo, &mAimPoseState_[i]), "Failed to get Pose State.")
        if (mAimPoseState_[i].isActive) {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            XrResult res = xrLocateSpace(mAimPoseSpace_[i], mLocalSpace_, predictedTime, &spaceLocation);
            if (XR_UNQUALIFIED_SUCCESS(res) &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                mAimPose_[i] = spaceLocation.pose;
            } else {
                mAimPoseState_[i].isActive = false;
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        actionStateGetInfo.action = mGrabAction_;
        actionStateGetInfo.subactionPath = mHandPaths_[i];
        OPENXR_CHECK(xrGetActionStateFloat(mSession_, &actionStateGetInfo, &mGrabState_[i]), "Failed to get Float State of Grab action.")
    }
    for (int i = 0; i < 2; i++) {
        actionStateGetInfo.action = mTriggerAction_;
        actionStateGetInfo.subactionPath = mHandPaths_[i];
        OPENXR_CHECK(xrGetActionStateFloat(mSession_, &actionStateGetInfo, &mTriggerState_[i]), "Failed to get Float Trigger action.")
    }
    // button states
    for (int i = 0; i < static_cast<int>(InputHandlerXR::Button::LAST); ++i) {
        actionStateGetInfo.action = mButtonActions_[i];
        actionStateGetInfo.subactionPath = 0;
        OPENXR_CHECK(xrGetActionStateBoolean(mSession_, &actionStateGetInfo, &mButtonStates_[i]), "Failed to get Boolean State of " + i +" Button action.")
    }

    // joystick direction
    for (int i = 0; i < 2; i++) {
        actionStateGetInfo.action = mJoystickDirAction_;
        actionStateGetInfo.subactionPath = mHandPaths_[i];
        OPENXR_CHECK(xrGetActionStateVector2f(mSession_, &actionStateGetInfo, &mJoystickDirState_[i]), "Failed to get Vec2 State of Change Color action.")
    }
}

const XrPosef& InputHandlerXR::getAimPose(InputHandlerXR::Hand hand) const {
    return mAimPose_[static_cast<int>(hand)];
}

const XrPosef& InputHandlerXR::getGripPose(InputHandlerXR::Hand hand) const {
    return mGripPose_[static_cast<int>(hand)];
}

const XrPosef& InputHandlerXR::getHeadPose() const {
    return mHeadPose_;
}

const XrVector2f& InputHandlerXR::getJoystickDirection(Hand hand) const {
    return mJoystickDirState_[static_cast<int>(hand)].currentState;
}

bool InputHandlerXR::getButtonDown(InputHandlerXR::Button button) const {
    return mButtonStates_[static_cast<int>(button)].currentState;
}

float InputHandlerXR::getTriggerState(InputHandlerXR::Hand hand) const {
    return mTriggerState_[static_cast<int>(hand)].currentState;
}

float InputHandlerXR::getGrabState(InputHandlerXR::Hand hand) const {
    return mGrabState_[static_cast<int>(hand)].currentState;
}

}
#endif