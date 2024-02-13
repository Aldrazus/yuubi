#pragma once

#include "pch.h"
#include "event/event.h"

class WindowCloseEvent : public Event {
    public:
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
        EVENT_CLASS_TYPE(WindowClose)
        
        WindowCloseEvent() {}
};

class WindowResizeEvent : public Event {
    public:
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
        EVENT_CLASS_TYPE(WindowResize)

        WindowResizeEvent(int width, int height)
            : width_(width), height_(height) {}

        int32_t getWidth() const {
            return width_;
        }

        int32_t getHeight() const {
            return height_;
        }
    private:
        int32_t width_;
        int32_t height_;
};
