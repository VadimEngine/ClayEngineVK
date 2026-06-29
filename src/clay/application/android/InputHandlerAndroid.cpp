#ifdef CLAY_PLATFORM_ANDROID
#include "clay/application/android/InputHandlerAndroid.h"

namespace clay {

// TouchEvent implementation

InputHandlerAndroid::TouchEvent::TouchEvent(Type type, int32_t pointerId, const glm::vec2& position, float pressure)
    : mType_(type), mPointerId_(pointerId), mPosition_(position), mPressure_(pressure) {
}

InputHandlerAndroid::TouchEvent::Type InputHandlerAndroid::TouchEvent::getType() const {
    return mType_;
}

int32_t InputHandlerAndroid::TouchEvent::getPointerId() const {
    return mPointerId_;
}

glm::vec2 InputHandlerAndroid::TouchEvent::getPosition() const {
    return mPosition_;
}

float InputHandlerAndroid::TouchEvent::getPressure() const {
    return mPressure_;
}

// InputHandlerAndroid implementation

InputHandlerAndroid::InputHandlerAndroid() {
    // Initialize all pointer states as inactive
    for (auto& state : mPointerStates_) {
        state.active = false;
    }
}

InputHandlerAndroid::~InputHandlerAndroid() {}

bool InputHandlerAndroid::processInputEvent(AInputEvent* event) {
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        processMotionEvent(event);
        return true;
    }
    // Could add key event handling here if needed for hardware keys
    return false;
}

void InputHandlerAndroid::processMotionEvent(AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) 
                            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    
    size_t pointerCount = AMotionEvent_getPointerCount(event);

    switch (actionMasked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            // New pointer down
            int32_t pointerId = AMotionEvent_getPointerId(event, pointerIndex);
            if (pointerId >= 0 && pointerId < kMaxPointers) {
                float x = AMotionEvent_getX(event, pointerIndex);
                float y = AMotionEvent_getY(event, pointerIndex);
                float pressure = AMotionEvent_getPressure(event, pointerIndex);

                mPointerStates_[pointerId].active = true;
                mPointerStates_[pointerId].position = {x, y};
                mPointerStates_[pointerId].pressure = pressure;

                mTouchEventQueue_.push(TouchEvent(
                    TouchEvent::Type::DOWN,
                    pointerId,
                    {x, y},
                    pressure
                ));
                trimBuffer();
            }
            break;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            // Pointer released
            int32_t pointerId = AMotionEvent_getPointerId(event, pointerIndex);
            if (pointerId >= 0 && pointerId < kMaxPointers) {
                float x = AMotionEvent_getX(event, pointerIndex);
                float y = AMotionEvent_getY(event, pointerIndex);

                mPointerStates_[pointerId].active = false;

                mTouchEventQueue_.push(TouchEvent(
                    TouchEvent::Type::UP,
                    pointerId,
                    {x, y},
                    mPointerStates_[pointerId].pressure
                ));
                trimBuffer();
            }
            break;
        }

        case AMOTION_EVENT_ACTION_MOVE: {
            // One or more pointers moved
            for (size_t i = 0; i < pointerCount; ++i) {
                int32_t pointerId = AMotionEvent_getPointerId(event, i);
                if (pointerId >= 0 && pointerId < kMaxPointers && mPointerStates_[pointerId].active) {
                    float x = AMotionEvent_getX(event, i);
                    float y = AMotionEvent_getY(event, i);
                    float pressure = AMotionEvent_getPressure(event, i);

                    mPointerStates_[pointerId].position = {x, y};
                    mPointerStates_[pointerId].pressure = pressure;

                    mTouchEventQueue_.push(TouchEvent(
                        TouchEvent::Type::MOVE,
                        pointerId,
                        {x, y},
                        pressure
                    ));
                }
            }
            trimBuffer();
            break;
        }

        case AMOTION_EVENT_ACTION_CANCEL: {
            // Touch cancelled - clear all pointers
            for (size_t i = 0; i < pointerCount; ++i) {
                int32_t pointerId = AMotionEvent_getPointerId(event, i);
                if (pointerId >= 0 && pointerId < kMaxPointers) {
                    float x = AMotionEvent_getX(event, i);
                    float y = AMotionEvent_getY(event, i);

                    mPointerStates_[pointerId].active = false;

                    mTouchEventQueue_.push(TouchEvent(
                        TouchEvent::Type::CANCEL,
                        pointerId,
                        {x, y},
                        mPointerStates_[pointerId].pressure
                    ));
                }
            }
            trimBuffer();
            break;
        }
    }
}

std::optional<InputHandlerAndroid::TouchEvent> InputHandlerAndroid::getTouchEvent() {
    if (!mTouchEventQueue_.empty()) {
        TouchEvent e = mTouchEventQueue_.front();
        mTouchEventQueue_.pop();
        return e;
    }
    return std::nullopt;
}

bool InputHandlerAndroid::isPointerDown(int32_t pointerId) const {
    if (pointerId >= 0 && pointerId < kMaxPointers) {
        return mPointerStates_[pointerId].active;
    }
    return false;
}

std::optional<glm::vec2> InputHandlerAndroid::getPointerPosition(int32_t pointerId) const {
    if (pointerId >= 0 && pointerId < kMaxPointers && mPointerStates_[pointerId].active) {
        return mPointerStates_[pointerId].position;
    }
    return std::nullopt;
}

std::vector<int32_t> InputHandlerAndroid::getActivePointers() const {
    std::vector<int32_t> activePointers;
    for (int32_t i = 0; i < kMaxPointers; ++i) {
        if (mPointerStates_[i].active) {
            activePointers.push_back(i);
        }
    }
    return activePointers;
}

void InputHandlerAndroid::clearTouchState() {
    // Clear all pointer states
    for (auto& state : mPointerStates_) {
        state.active = false;
    }
    
    // Clear event queue
    while (!mTouchEventQueue_.empty()) {
        mTouchEventQueue_.pop();
    }
}

void InputHandlerAndroid::trimBuffer() {
    while (mTouchEventQueue_.size() > kMaxQueueSize) {
        mTouchEventQueue_.pop();
    }
}

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID
