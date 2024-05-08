module;

#include "event/event.h"

export module Yuubi.Event:Key;

import :Base;
import :Enums;
import :KeyCodes;

export class KeyEvent : public Event {
    public:
    EVENT_CLASS_CATEGORY(EventCategoryKeyboard)

    protected:
        KeyEvent(const KeyCode keycode)
            : keyCode(keycode) {}

        KeyCode keyCode;
};

export class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(const KeyCode keycode, bool repeat = false) : KeyEvent(keycode), repeat(repeat) {}
        bool isRepeating() const {
            return this->repeat;
        }
        EVENT_CLASS_TYPE(KeyPressed)

    private:
    bool repeat;
};

export class KeyReleasedEvent : public KeyEvent {
    public:
        KeyReleasedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
        EVENT_CLASS_TYPE(KeyReleased)
};

export class KeyTypedEvent : public KeyEvent {
    public:
        KeyTypedEvent(const KeyCode keycode) : KeyEvent(keycode) {}
        EVENT_CLASS_TYPE(KeyTyped)
};
