#pragma once

#include "mouse_codes.h"
#include "pch.h"
#include "event.h"

class MouseButtonEvent : public Event {
public:
    EVENT_CLASS_CATEGORY(EventCategoryMouseButton | EventCategoryMouse | EventCategoryInput)
    MouseCode mouseCode;

protected:
    explicit MouseButtonEvent(const MouseCode mouseCode) : mouseCode(mouseCode) {}
};

class MouseButtonPressedEvent final : public MouseButtonEvent {
public:
    EVENT_CLASS_TYPE(MouseButtonPressed)
    explicit MouseButtonPressedEvent(const MouseCode mouseCode) : MouseButtonEvent(mouseCode) {}
};

class MouseButtonReleasedEvent final : public MouseButtonEvent {
public:
    EVENT_CLASS_TYPE(MouseButtonReleased)
    explicit MouseButtonReleasedEvent(const MouseCode mouseCode) : MouseButtonEvent(mouseCode) {}
};

class MouseScrollEvent final : public Event {
public:
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    EVENT_CLASS_TYPE(MouseMoved)

    MouseScrollEvent(const double xOffset, const double yOffset) : xOffset(xOffset), yOffset(yOffset) {}
    double xOffset;
    double yOffset;
};

class MouseMovedEvent final : public Event {
public:
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    EVENT_CLASS_TYPE(MouseMoved)

    MouseMovedEvent(const double xPos, const double yPos) : xPos(xPos), yPos(yPos) {}
    double xPos;
    double yPos;
};
