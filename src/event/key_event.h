#pragma once

#include "event/event.h"
#include "key_codes.h"

class KeyEvent : public Event {
public:
    EVENT_CLASS_CATEGORY(EventCategoryKeyboard)

    KeyCode keyCode;

protected:
    explicit KeyEvent(const KeyCode keycode) : keyCode(keycode) {}
};

class KeyPressedEvent final : public KeyEvent {
public:
    explicit KeyPressedEvent(const KeyCode keycode, bool repeat = false) : KeyEvent(keycode), repeat(repeat) {}
    [[nodiscard]] bool isRepeating() const { return this->repeat; }
    EVENT_CLASS_TYPE(KeyPressed)

private:
    bool repeat;
};

class KeyReleasedEvent final : public KeyEvent {
public:
    explicit KeyReleasedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
    EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent final : public KeyEvent {
public:
    explicit KeyTypedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
    EVENT_CLASS_TYPE(KeyTyped)
};
