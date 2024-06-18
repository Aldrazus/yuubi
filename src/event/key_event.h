#pragma once

#include "event/event.h"
#include "key_codes.h"

class KeyEvent : public Event {
    public:
    EVENT_CLASS_CATEGORY(EventCategoryKeyboard)

    KeyCode keyCode;

    protected:
        KeyEvent(const KeyCode keycode)
            : keyCode(keycode) {}

};

class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(const KeyCode keycode, bool repeat = false) : KeyEvent(keycode), repeat(repeat) {}
        bool isRepeating() const {
            return this->repeat;
        }
        EVENT_CLASS_TYPE(KeyPressed)

    private:
    bool repeat;
};

class KeyReleasedEvent : public KeyEvent {
    public:
        KeyReleasedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
        EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent : public KeyEvent {
    public:
        KeyTypedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
        EVENT_CLASS_TYPE(KeyTyped)
};
