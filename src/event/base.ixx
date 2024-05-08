module;

#include <cstdint>

export module Yuubi.Event:Base;

import :Enums;

export class Event {
    public:
        virtual ~Event() = default;

        static EventType getStaticType();
        virtual EventType getEventType() const = 0;
        virtual uint16_t getCategoryFlags() const = 0;
        virtual const char* getName() const = 0;
        bool isInCategory(EventCategory category) {
            return getCategoryFlags() & category;
        }
        
        bool handled = false;
    protected:
        Event() = default;
};
