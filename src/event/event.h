#pragma once

#include <cstdint>

#define EVENT_CLASS_TYPE(type) static EventType getStaticType() { return EventType::type; }\
    virtual EventType getEventType() const override { return getStaticType(); }\
    virtual const char* getName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual uint16_t getCategoryFlags() const override { return category; }
