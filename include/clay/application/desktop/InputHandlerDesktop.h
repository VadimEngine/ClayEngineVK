#pragma once

#ifdef CLAY_PLATFORM_DESKTOP

// standard lib
#include <bitset>
#include <optional>
#include <queue>
// third party
#include <glm/vec2.hpp>

namespace clay {

class InputHandlerDesktop {
public:
    class KeyEvent {
    public:
        enum class Type {
            PRESS, RELEASE
        };
    private:
        Type mType_;
        unsigned int mCode_;
    public:
        KeyEvent(Type type, unsigned int code);
        Type getType() const;
        unsigned int getCode() const;
    };

    class MouseEvent {
    public:
        enum class Type {
            PRESS,
            RELEASE,
            SCROLL_UP,
            SCROLL_DOWN,
            MOVE,
            ENTER,
            LEAVE,
        };
        enum class Button {
            LEFT=0, MIDDLE, RIGHT, NUM_BUTTONS, NONE
        };
    private:
        Type mType_;
        Button button;
        glm::ivec2 mPosition_;
    public:
        MouseEvent(Type type, Button button, const glm::ivec2& position);
        Type getType() const;
        Button getButton() const;
        glm::ivec2 getPosition() const;
    };

public:
    /** Constructor */
    InputHandlerDesktop();

    /** Destructor*/
    ~InputHandlerDesktop();

    /**
     * Track the key that is pressed
     * @param keyCode Keycode for pressed key
     */
    void onKeyPressed(int keyCode);

    /**
     * Track the key that is released
     * @param keyCode Keycode for pressed key
     */
    void onKeyReleased(int keyCode);

    /** Check if a key is marked as pressed */
    bool isKeyPressed(int keyCode) const;

    /** Set all tracked keys as not pressed. Used when losing focus on application */
    void clearKeys();

    /** */
    std::optional<KeyEvent> getKeyEvent();

    bool isMouseButtonPressed(MouseEvent::Button button);

    std::optional<MouseEvent> getMouseEvent();

    void onMousePress(MouseEvent::Button button);

    void onMouseRelease(MouseEvent::Button button);

    void onMouseMove(int x, int y);

    glm::ivec2 getMousePosition();

    void onMouseWheel(float yOffset);

private:
    /** Helper method to keep keyBuffer within the size limit */
    template<typename T>
    void trimBuffer(T eventQueue);

    /** Number of key codes to check status of*/
    static constexpr unsigned int kKeys = 512;
    /** Max number of key events in the event queue */
    static constexpr unsigned int kMaxQueueSize = 16;

    /** Bitset to track which keys are currently pressed*/
    std::bitset<kKeys> mKeyStates_;
    /** Queue of key events to use for single updates on a key event */
    std::queue<KeyEvent> mKeyEventQueue_;

    /** Current mouse position */
    glm::ivec2 mMousePosition_;

    /** Bitset to track current mouse button state */
    std::bitset<3> mMouseStates_;
    /** Queue of mouse events to use for single updates on a mouse event */
    std::queue<MouseEvent> mMouseEventQueue_;
};

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP