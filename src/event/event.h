#pragma once

#include "pch.h"

// TODO: make private
constexpr uint8_t bit(uint8_t n) { return 1 << n; }

enum class EventType {
    None = 0,
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    WindowMoved,
    AppTick,
    AppUpdate,
    AppRender,
    KeyPressed,
    KeyReleased,
    KeyTyped,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled
};

enum EventCategory {
    None = 0,
    EventCategoryApplication = bit(0),
    EventCategoryInput = bit(1),
    EventCategoryKeyboard = bit(2),
    EventCategoryMouse = bit(3),
    EventCategoryMouseButton = bit(4)
};

#define EVENT_CLASS_TYPE(type)                                                  \
    static EventType getStaticType() { return EventType::type; }                \
    virtual EventType getEventType() const override { return getStaticType(); } \
    virtual const char* getName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
    virtual uint16_t getCategoryFlags() const override { return category; }

class Event {
public:
    virtual ~Event() = default;

    static EventType getStaticType();
    [[nodiscard]] virtual EventType getEventType() const = 0;
    [[nodiscard]] virtual uint16_t getCategoryFlags() const = 0;
    [[nodiscard]] virtual const char* getName() const = 0;
    [[nodiscard]] bool isInCategory(EventCategory category) const { return (getCategoryFlags() & category) != 0; }

    bool handled = false;

protected:
    Event() = default;
};

// TODO: support multiple callbacks for single event
// Just use inheritance and have each callback accept
// base class and downcast internally
class EventDispatcher {
public:
    explicit EventDispatcher(Event& event) : event(event) {}

    template<typename T, typename F>
    bool dispatch(const F& func) {
        if (this->event.getEventType() == T::getStaticType()) {
            this->event.handled |= func(static_cast<T&>(this->event));
            return true;
        }
        return false;
    }

private:
    Event& event;
};
