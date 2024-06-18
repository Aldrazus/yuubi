#pragma once

#include "mouse_codes.h"
#include "pch.h"
#include "event.h"

class MouseButtonEvent : public Event {
    public:
    EVENT_CLASS_CATEGORY(EventCategoryMouseButton | EventCategoryMouse | EventCategoryInput)
    MouseCode mouseCode;

    protected:
MouseButtonEvent(const MouseCode mousecode) : mouseCode(mousecode) {}
};

class MouseButtonPressedEvent : public MouseButtonEvent {
    public:
        EVENT_CLASS_TYPE(MouseButtonPressed)
        MouseButtonPressedEvent(const MouseCode mousecode) : MouseButtonEvent(mousecode) {}
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
    public:
        EVENT_CLASS_TYPE(MouseButtonReleased)
        MouseButtonReleasedEvent(const MouseCode mousecode) : MouseButtonEvent(mousecode) {}
};

class MouseScrollEvent : public Event {
    public:
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
        EVENT_CLASS_TYPE(MouseMoved)

        MouseScrollEvent(const double xoffset, const double yoffset) : xOffset(xoffset), yOffset(yoffset) {}
        double xOffset;
        double yOffset;
};

class MouseMovedEvent : public Event {
    public:
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
        EVENT_CLASS_TYPE(MouseMoved)

        MouseMovedEvent(const double xpos, const double ypos) : xPos(xpos), yPos(ypos) {}
        double xPos;
        double yPos;

};
