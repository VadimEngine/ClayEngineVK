#pragma once

#ifdef CLAY_PLATFORM_ANDROID

// standard lib
#include <optional>
#include <queue>
#include <vector>
// third party
#include <glm/vec2.hpp>
#include <android/input.h>

namespace clay {

class InputHandlerAndroid {
public:
    class TouchEvent {
    public:
        enum class Type {
            DOWN,      // Finger pressed down
            UP,        // Finger lifted
            MOVE,      // Finger moved
            CANCEL     // Touch cancelled
        };
    private:
        Type mType_;
        int32_t mPointerId_;
        glm::vec2 mPosition_;
        float mPressure_;
    public:
        TouchEvent(Type type, int32_t pointerId, const glm::vec2& position, float pressure = 1.0f);
        Type getType() const;
        int32_t getPointerId() const;
        glm::vec2 getPosition() const;
        float getPressure() const;
    };

public:
    /** Constructor */
    InputHandlerAndroid();

    /** Destructor*/
    ~InputHandlerAndroid();

    /**
     * Process an Android input event
     * @param event The AInputEvent from Android
     * @return true if event was handled
     */
    bool processInputEvent(AInputEvent* event);

    /**
     * Get the next touch event from the queue
     * @return Optional touch event, nullopt if queue is empty
     */
    std::optional<TouchEvent> getTouchEvent();

    /**
     * Check if a specific pointer is currently touching the screen
     * @param pointerId The pointer ID to check
     * @return true if pointer is down
     */
    bool isPointerDown(int32_t pointerId) const;

    /**
     * Get the position of a specific pointer
     * @param pointerId The pointer ID to query
     * @return Optional position, nullopt if pointer not found
     */
    std::optional<glm::vec2> getPointerPosition(int32_t pointerId) const;

    /**
     * Get all currently active pointer IDs
     * @return Vector of active pointer IDs
     */
    std::vector<int32_t> getActivePointers() const;

    /**
     * Clear all touch state (useful when app loses focus)
     */
    void clearTouchState();

private:
    /** Helper method to keep event queue within the size limit */
    void trimBuffer();

    /** Process a motion event (touch) */
    void processMotionEvent(AInputEvent* event);

    /** Max number of touch events in the event queue */
    static constexpr unsigned int kMaxQueueSize = 32;

    /** Max number of simultaneous pointers to track */
    static constexpr unsigned int kMaxPointers = 10;

    /** Queue of touch events */
    std::queue<TouchEvent> mTouchEventQueue_;

    /** Structure to track active pointer state */
    struct PointerState {
        bool active = false;
        glm::vec2 position = {0.0f, 0.0f};
        float pressure = 1.0f;
    };

    /** Array of pointer states indexed by pointer ID */
    PointerState mPointerStates_[kMaxPointers];
};

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID